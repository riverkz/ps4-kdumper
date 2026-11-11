//#define DEBUG_SOCKET
#define DEBUG_IP "192.168.2.2"
#define DEBUG_PORT 9023

#include "ps4.h"

#define KERNEL_CHUNK_SIZE PAGE_SIZE

// PS4 kernel text is linked at this fixed virtual address on every firmware
// (including 14.00), so it is used to normalise the kernel segment virtual
// addresses read out of the ELF header and to compute the dump size portably.
#define KERNEL_BASE_VADDR 0xFFFFFFFF82200000ULL

// Bounds of the ELF header we are willing to trust. On 14.00 a base that came
// from a firmware the SDK does not know would otherwise walk garbage segment
// headers and write a nonsense sized dump to the USB device.
#define ELF_HEADER_SIZE_MIN 0x34
#define ELF_HEADER_SIZE_MAX 0x1000
#define ELF_ENTRY_SIZE_MIN 0x38
#define ELF_ENTRY_SIZE_MAX 0x100
#define ELF_ENTRIES_MAX 64
#define KERNEL_SIZE_MAX 0x4000000ULL // 64 MiB, the kernel image is far below this

int nthread_run = 1;
int notify_time = 20;
char notify_buf[512] = {0};

void *nthread_func(void *arg) {
  UNUSED(arg);
  time_t t1 = 0;
  while (nthread_run) {
    if (notify_buf[0]) {
      time_t t2 = time(NULL);
      if ((t2 - t1) >= notify_time) {
        t1 = t2;
        printf_notification("%s", notify_buf);
      }
    } else {
      t1 = 0;
    }
    sceKernelSleep(1);
  }
  return NULL;
}

uint64_t get_kernel_size(uint64_t kernel_base) {
  uint16_t elf_header_size;       // ELF header size
  uint16_t elf_header_entry_size; // ELF header entry size
  uint16_t num_of_elf_entries;    // Number of entries in the ELF header

  get_memory_dump(kernel_base + 0x34, (uint64_t *)&elf_header_size, sizeof(uint16_t));
  get_memory_dump(kernel_base + 0x34 + sizeof(uint16_t), (uint64_t *)&elf_header_entry_size, sizeof(uint16_t));
  get_memory_dump(kernel_base + 0x34 + (sizeof(uint16_t) * 2), (uint64_t *)&num_of_elf_entries, sizeof(uint16_t));

  printf_debug("elf_header_size: %u bytes\n", elf_header_size);
  printf_debug("elf_header_entry_size: %u bytes\n", elf_header_entry_size);
  printf_debug("num_of_elf_entries: %u\n", num_of_elf_entries);

  if (elf_header_size < ELF_HEADER_SIZE_MIN || elf_header_size > ELF_HEADER_SIZE_MAX ||
      elf_header_entry_size < ELF_ENTRY_SIZE_MIN || elf_header_entry_size > ELF_ENTRY_SIZE_MAX ||
      num_of_elf_entries == 0 || num_of_elf_entries > ELF_ENTRIES_MAX) {
    printf_debug("ELF header at %p looks wrong, refusing to size the kernel\n", (void *)kernel_base);
    return 0;
  }

  uint64_t max = 0;
  for (int i = 0; i < num_of_elf_entries; i++) {
    uint64_t temp_memsz;
    uint64_t temp_vaddr;
    uint64_t temp_align;
    uint64_t temp_max;

    uint64_t memsz_offset = elf_header_size + (i * elf_header_entry_size) + 0x28;
    uint64_t vaddr_offset = elf_header_size + (i * elf_header_entry_size) + 0x10;
    uint64_t align_offset = elf_header_size + (i * elf_header_entry_size) + 0x30;
    get_memory_dump(kernel_base + memsz_offset, &temp_memsz, sizeof(uint64_t));
    get_memory_dump(kernel_base + vaddr_offset, &temp_vaddr, sizeof(uint64_t));
    get_memory_dump(kernel_base + align_offset, &temp_align, sizeof(uint64_t));

    if (temp_align == 0 || (temp_align & (temp_align - 1)) != 0) {
      printf_debug("segment %i has alignment %lu, skipping it\n", i, temp_align);
      continue;
    }

    temp_vaddr -= kernel_base;
    temp_vaddr += KERNEL_BASE_VADDR;

    temp_max = (temp_vaddr + temp_memsz + (temp_align - 1)) & ~(temp_align - 1);

    if (temp_max > max) {
      max = temp_max;
    }
  }

  return max > KERNEL_BASE_VADDR ? max - KERNEL_BASE_VADDR : 0;
}

int _main(struct thread *td) {
  UNUSED(td);

  char fw_version[8] = {0};
  char usb_name[7] = {0};
  char usb_path[13] = {0};
  char output_root[PATH_MAX] = {0};
  char saveFile[PATH_MAX] = {0};
  char completion_check[PATH_MAX] = {0};

  initKernel();
  initLibc();
  initPthread();

#ifdef DEBUG_SOCKET
  initNetwork();
  DEBUG_SOCK = SckConnect(DEBUG_IP, DEBUG_PORT);
#endif

  jailbreak();

  initSysUtil();

  if (get_firmware_string(fw_version) < 0) {
    printf_notification("Failed to detect firmware version! Quitting...");
    return 0;
  }

  uint64_t kernel_base = get_kernel_base();
  if (kernel_base == (uint64_t)-1) {
    printf_notification("Firmware %s is not supported by the SDK! Quitting...", fw_version);
    return 0;
  }

  ScePthread nthread;
  memset_s(&nthread, sizeof(ScePthread), 0, sizeof(ScePthread));
  scePthreadCreate(&nthread, NULL, nthread_func, NULL, "nthread");

  printf_notification("Running Kernel Dumper");

  snprintf_s(notify_buf, sizeof(notify_buf), "Waiting for USB device...");
  wait_for_usb(usb_name, usb_path);
  notify_buf[0] = '\0';

  snprintf_s(output_root, sizeof(output_root), "%s/PS4", usb_path);
  mkdir(output_root, 0777);
  snprintf_s(output_root, sizeof(output_root), "%s/%s", output_root, fw_version);
  mkdir(output_root, 0777);

  snprintf_s(saveFile, sizeof(saveFile), "%s/PS4/%s/kernel.bin", usb_path, fw_version);

  snprintf_s(completion_check, sizeof(completion_check), "%s/kernel.complete", output_root);
  if (file_exists(completion_check)) {
    printf_notification("Kernel already dumped for %s, skipping dumping", fw_version);
    return 0;
  } else {
    unlink(saveFile);
  }

  // Size the kernel before creating the file, so a bad base leaves nothing
  // behind on the USB device -- and so the early bail-out cannot drop a
  // zero-length kernel.bin next to a stale kernel.complete marker.
  uint64_t kernel_size = get_kernel_size(kernel_base);
  if (kernel_size == 0 || kernel_size > KERNEL_SIZE_MAX) {
    printf_notification("Could not size the kernel for %s (%lu bytes)! Quitting...", fw_version, kernel_size);
    return 0;
  }
  uint64_t num_of_kernel_chunks = (kernel_size + (KERNEL_CHUNK_SIZE / 2)) / KERNEL_CHUNK_SIZE;

  int fd = open(saveFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (fd < 0) {
    printf_notification("Unabled to create kernel dump! Quitting...");
    return 0;
  }

  printf_notification("USB device detected.\n\nStarting kernel dumping to %s.", usb_name);

  printf_debug("Kernel Size: %lu bytes\n", kernel_size);
  printf_debug("Kernel Chunks: %lu\n", num_of_kernel_chunks);

  notify_time = 5;
  uint64_t *dump = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  uint64_t pos = 0;
  for (uint64_t i = 0; i < num_of_kernel_chunks; i++) {
    get_memory_dump(kernel_base + pos, dump, KERNEL_CHUNK_SIZE);
    lseek(fd, pos, SEEK_SET);
    write(fd, (void *)dump, KERNEL_CHUNK_SIZE);
    int percent = ((double)(KERNEL_CHUNK_SIZE * i) / ((double)KERNEL_CHUNK_SIZE * (double)num_of_kernel_chunks)) * 100;
    snprintf_s(notify_buf, sizeof(notify_buf), "Kernel dumping to %s\nDone: %i%%", usb_name, percent);
    pos = pos + KERNEL_CHUNK_SIZE;
  }
  notify_buf[0] = '\0';
  nthread_run = 0;

  close(fd);
  munmap(dump, 0x4000);

  touch_file(completion_check);

  printf_notification("Kernel dumped successfully!");

#ifdef DEBUG_SOCKET
  printf_debug("Closing socket...\n");
  SckClose(DEBUG_SOCK);
#endif

  return 0;
}
