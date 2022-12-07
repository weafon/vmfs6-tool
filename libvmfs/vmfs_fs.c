/*
 * vmfs-tools - Tools to access VMFS filesystems
 * Copyright (C) 2009 Christophe Fillot <cf@utc.fr>
 * Copyright (C) 2009,2012 Mike Hommey <mh@glandium.org>
 * Copyright (C) 2018 Weafon Tsao <weafon.tsao@accelstor.com>
 * Copyright (C) 2020 VMware, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/* 
 * VMFS filesystem..
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "vmfs.h"

/* VMFS meta-files */
#define VMFS_FBB_FILENAME  ".fbb.sf"
#define VMFS_FDC_FILENAME  ".fdc.sf"
#define VMFS_PBC_FILENAME  ".pbc.sf"
#define VMFS_SBC_FILENAME  ".sbc.sf"
#define VMFS_PB2_FILENAME  ".pb2.sf"

/* Read a block from the filesystem */
ssize_t vmfs_fs_read(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                     u_char *buf,size_t len)
{
   off_t pos;

   pos  = (uint64_t)blk * vmfs_fs_get_blocksize(fs);
   pos += offset;

   return(vmfs_device_read(fs->dev,pos,buf,len));
}

/* Write a block to the filesystem */
ssize_t vmfs_fs_write(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                      const u_char *buf,size_t len)
{
   off_t pos;

   pos  = (uint64_t)blk * vmfs_fs_get_blocksize(fs);
   pos += offset;

   return(vmfs_device_write(fs->dev,pos,buf,len));
}

/* Read filesystem information */
static int vmfs_fsinfo_read(vmfs_fs_t *fs)
{
   DECL_ALIGNED_BUFFER(buf,512);
   vmfs_fsinfo_t *fsi = &fs->fs_info;

   if (vmfs_device_read(fs->dev,VMFS_FSINFO_BASE,buf,buf_len) != buf_len)
      return(-1);

   fsi->magic = read_le32(buf,VMFS_FSINFO_OFS_MAGIC);

   if (fsi->magic != VMFS_FSINFO_MAGIC && fsi->magic != VMFSL_FSINFO_MAGIC) {
      fprintf(stderr,"VMFS FSInfo: invalid magic number 0x%8.8x\n",fsi->magic);
      return(-1);
   }

   fsi->vol_version      = read_le32(buf,VMFS_FSINFO_OFS_VOLVER);
   fsi->version          = buf[VMFS_FSINFO_OFS_VER];
   fsi->mode             = read_le32(buf,VMFS_FSINFO_OFS_MODE);
   fsi->block_size       = read_le64(buf,VMFS_FSINFO_OFS_BLKSIZE);
   fsi->subblock_size    = read_le32(buf,VMFS_FSINFO_OFS_SBSIZE);
   fsi->fdc_header_size  = read_le32(buf,VMFS_FSINFO_OFS_FDC_HEADER_SIZE);
   fsi->fdc_bitmap_count = read_le32(buf,VMFS_FSINFO_OFS_FDC_BITMAP_COUNT);
   fsi->ctime            = (time_t)read_le32(buf,VMFS_FSINFO_OFS_CTIME);

   read_uuid(buf,VMFS_FSINFO_OFS_UUID,&fsi->uuid);
   fsi->label = strndup((char *)buf+VMFS_FSINFO_OFS_LABEL,
                        VMFS_FSINFO_OFS_LABEL_SIZE);
   read_uuid(buf,VMFS_FSINFO_OFS_LVM_UUID,&fsi->lvm_uuid);

   return(0);
}

static vmfs_bitmap_t *vmfs_open_meta_file(vmfs_dir_t *root_dir, char *name,
                                          uint32_t max_item, uint32_t max_entry,
                                          char *desc)
{
   vmfs_bitmap_t *bitmap;

   dprintf("%s : call for metafile %s\n", __FUNCTION__, name);
   bitmap = vmfs_bitmap_open_at(root_dir, name);
   if (!bitmap) {
      fprintf(stderr, "Unable to open %s.\n", desc);
      return NULL;
   }

   if (bitmap->bmh.items_per_bitmap_entry > max_item) {
      fprintf(stderr, "Unsupported number of items per entry in %s. %u %u\n", desc, bitmap->bmh.items_per_bitmap_entry, max_item);
      return NULL;
   }
#if 0   
   if ((bitmap->bmh.total_items==0)&&(strcmp(name,".pbd.sf")==0)) // since pbc.sf may be replaced by .sbc.sf
   {
   		bitmap->bmh.items_per_bitmap_entry = 0x100;
   		bitmap->bmh.total_items = bitmap->bmh.items_per_bitmap_entry* bitmap->bmh.bmp_entries_per_area*bitmap->bmh.area_count;
   		printf("Force to set items to 0x100\n");
   		   	return bitmap;
   	}
#endif
	if (bitmap->bmh.total_items==0) // if the bitmap is disabled then total_items will be 0. e.g. pbc.sf in vmfs6
		return bitmap;
		
   if ((bitmap->bmh.total_items + bitmap->bmh.items_per_bitmap_entry - 1) /
        bitmap->bmh.items_per_bitmap_entry > max_entry) {
      fprintf(stderr,"Unsupported number of entries in %s.\n", desc);
      return NULL;
   }
   return bitmap;
}

/* Open all the VMFS meta files */
static int vmfs_open_all_meta_files(vmfs_fs_t *fs)
{
   vmfs_bitmap_t *fdc = fs->fdc;
   vmfs_dir_t *root_dir;

   /* Read the first inode */
   if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0, 0, 0)))) {
      fprintf(stderr,"VMFS: unable to open root directory\n");
      return(-1);
   }
   dprintf("Open root dir done\n");

   dprintf("open pb\n");
   fs->pbc = vmfs_open_meta_file(root_dir, VMFS_PBC_FILENAME,
                                 VMFS_BLK_PB_MAX_ITEM, VMFS_BLK_PB_MAX_ENTRY,
                                 "pointer block bitmap (PBC)");
   if (!fs->pbc)
      return(-1);

   dprintf("open pb2\n");
   fs->pb2 = vmfs_open_meta_file(root_dir, VMFS_PB2_FILENAME,
                                 VMFS_BLK_PB2_MAX_ITEM, VMFS_BLK_PB2_MAX_ENTRY,
                                 "pointer 2nd block bitmap (PB2)");
   if (!fs->pb2)
      return(-1);

   dprintf("open sbc\n");
   fs->sbc = vmfs_open_meta_file(root_dir, VMFS_SBC_FILENAME,
                                 VMFS_BLK_SB_MAX_ITEM, VMFS_BLK_SB_MAX_ENTRY,
                                 "pointer sub bitmap (SBC)");
   if (!fs->sbc)
      return(-1);

   if (!(fs->fbb = vmfs_bitmap_open_at(root_dir,VMFS_FBB_FILENAME))) {
      fprintf(stderr,"Unable to open file-block bitmap (FBB).\n");
      return(-1);
   }
   if (fs->fbb->bmh.total_items > VMFS_BLK_FB_MAX_ITEM) {
      fprintf(stderr, "Unsupported number of items in file-block bitmap (FBB) (0x%x 0x%lx).\n", fs->fbb->bmh.total_items, VMFS_BLK_FB_MAX_ITEM);
      return(-1);
   }

   fs->fdc = vmfs_open_meta_file(root_dir, VMFS_FDC_FILENAME,
                                 VMFS_BLK_FD_MAX_ITEM, VMFS_BLK_FD_MAX_ENTRY,
                                 "file descriptor bitmap (FDC)");
   if (!fs->fdc)
      return(-1);



   vmfs_bitmap_close(fdc);
   vmfs_dir_close(root_dir);
   return(0);
}

/* Read FDC base information */
static int vmfs_read_fdc_base(vmfs_fs_t *fs)
{
   vmfs_inode_t inode = { { 0, }, };
   uint64_t fdc_base;

   /* 
    * Compute position of FDC base: it is located at the first
    * block after heartbeat information.
    * When blocksize = 8 Mb, there is free space between heartbeats
    * and FDC.
    */
   dprintf("VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE = %d blocksize %lu\n", 
      VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE, vmfs_fs_get_blocksize(fs));
   fdc_base = m_max(1, (VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE) /
                    vmfs_fs_get_blocksize(fs));

   if (fs->debug_level > 0)
      printf("FDC base = block #%lu\n", fdc_base);

   inode.fs = fs;
   inode.mdh.magic = VMFS_INODE_MAGIC;
   inode.size = fs->fs_info.block_size;
   inode.type = VMFS_FILE_TYPE_META;
   inode.blk_size = fs->fs_info.block_size;
   inode.blk_count = 1;
   inode.zla = VMFS_BLK_TYPE_FB;
   inode.blocks[0] = VMFS_BLK_FB_BUILD(fdc_base, 0);
   inode.ref_count = 1;
   dprintf("fdc_base %lu blocks0 %lx (%lx %lx shift %d)\n", fdc_base, inode.blocks[0], 
   	VMFS_BLK_VALUE(fdc_base, VMFS_BLK_FB_ITEM_VALUE_LSB_MASK),
   	VMFS_BLK_FILL(VMFS_BLK_VALUE(fdc_base, VMFS_BLK_FB_ITEM_VALUE_LSB_MASK), VMFS_BLK_FB_ITEM_LSB_MASK),
   	VMFS_BLK_SHIFT(VMFS_BLK_FB_ITEM_LSB_MASK));

   fs->fdc = vmfs_bitmap_open_from_inode(&inode);

   /* Read the meta files */
   if (vmfs_open_all_meta_files(fs) == -1)
      return(-1);

   return(0);
}

static vmfs_device_t *vmfs_device_open(char **paths, vmfs_flags_t flags)
{
   vmfs_lvm_t *lvm;

   if (!(lvm = vmfs_lvm_create(flags))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      return NULL;
   }

   for (; *paths; paths++) {
      if (vmfs_lvm_add_extent(lvm, vmfs_vol_open(*paths, flags)) == -1) {
         fprintf(stderr,"Unable to open device/file \"%s\".\n",*paths);
         return NULL;
      }
   }

   if (vmfs_lvm_open(lvm)) {
      vmfs_device_close(&lvm->dev);
      return NULL;
   }

   return &lvm->dev;
}

/* Open a filesystem */
vmfs_fs_t *vmfs_fs_open(char **paths, vmfs_flags_t flags)
{
   vmfs_device_t *dev;
   vmfs_fs_t *fs;

   vmfs_host_init();

   dev = vmfs_device_open(paths, flags);

   if (!dev || !(fs = calloc(1,sizeof(*fs))))
      return NULL;

   fs->inode_hash_buckets = VMFS_INODE_HASH_BUCKETS;
   fs->inodes = calloc(fs->inode_hash_buckets,sizeof(vmfs_inode_t *));

   if (!fs->inodes) {
      free(fs);
      return NULL;
   }

   fs->dev = dev;
   fs->debug_level = flags.debug_level;

   /* Read FS info */
   if (vmfs_fsinfo_read(fs) == -1) {
      fprintf(stderr,"VMFS: Unable to read FS information\n");
      vmfs_fs_close(fs);
      return NULL;
   }

   if (uuid_compare(fs->fs_info.lvm_uuid, *fs->dev->uuid)) {
      fprintf(stderr,"VMFS: FS doesn't belong to the underlying LVM\n");
      vmfs_fs_close(fs);
      return NULL;
   }

   /* Read FDC base information */
   if (vmfs_read_fdc_base(fs) == -1) {
      fprintf(stderr,"VMFS: Unable to read FDC information\n");
      vmfs_fs_close(fs);
      return NULL;
   }

   if (fs->debug_level > 0)
      printf("VMFS: filesystem opened successfully\n");
   return fs;
}

/* 
 * Check that all inodes have been released, and synchronize them if this 
 * is not the case. 
 */
static void vmfs_fs_sync_inodes(vmfs_fs_t *fs)
{
   vmfs_inode_t *inode;
   int i;

   for(i=0;i<VMFS_INODE_HASH_BUCKETS;i++) {
      for(inode=fs->inodes[i];inode;inode=inode->next) {
#if 0
         printf("Inode 0x%8.8x: ref_count=%u, update_flags=0x%x\n",
                inode->id,inode->ref_count,inode->update_flags);
#endif
         if (inode->update_flags)
            vmfs_inode_update(inode,inode->update_flags & VMFS_INODE_SYNC_BLK);
      }
   }
}

/* Close a FS */
void vmfs_fs_close(vmfs_fs_t *fs)
{
   if (!fs)
      return;

   if (fs->hb_refcount > 0) {
      fprintf(stderr,
              "Warning: heartbeat still active in metadata (ref_count=%u)\n",
              fs->hb_refcount);
   }

   vmfs_heartbeat_unlock(fs,&fs->hb);

   vmfs_bitmap_close(fs->fbb);
   vmfs_bitmap_close(fs->fdc);
   vmfs_bitmap_close(fs->pbc);
   vmfs_bitmap_close(fs->pb2);   
   vmfs_bitmap_close(fs->sbc);

   vmfs_fs_sync_inodes(fs);

   vmfs_device_close(fs->dev);
   free(fs->inodes);
   free(fs->fs_info.label);
   free(fs);
}
