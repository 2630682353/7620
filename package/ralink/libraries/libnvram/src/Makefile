#
# (C) Copyright 2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#



MAJOR_VERSION := 0
MINOR_VERSION := 9
SUBLEVEL      := 28
LIBNVRAM=libnvram.a
LIBNVRAM_SHARED=libnvram.so
LIBNVRAM_SHARED_FULLNAME=libnvram-$(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL).so
OBJS	:= crc32.o nvram_env.o flash_api.o
HEADERS := nvram_env.h nvram.h flash_api.h
LN:=ln

all:	$(LIBNVRAM) $(LIBNVRAM_SHARED)


$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@

$(LIBNVRAM_SHARED) : $(OBJS)
	$(LD) $(LDFLAGS) -shared -soname=$(LIBNVRAM_SHARED).$(MAJOR_VERSION) \
		-o $(LIBNVRAM_SHARED_FULLNAME) --whole-archive $(LIBNVRAM) \
		--no-whole-archive -lc 
	$(LN) -sf $(LIBNVRAM_SHARED_FULLNAME) $(LIBNVRAM_SHARED)
	$(LN) -sf $(LIBNVRAM_SHARED_FULLNAME) $(LIBNVRAM_SHARED).$(MAJOR_VERSION)

$(LIBNVRAM) : $(OBJS)
	$(AR) $(ARFLAGS) $(LIBNVRAM) $(OBJS)

clean:
	$(RM) *.[oa] $(LIBNVRAM_SHARED)* $(LIBNVRAM_SHARED_FULLNAME)*

