/*
 * VMFS LVM layer
 */

#include <stdlib.h>
#include "vmfs.h"

#define VMFS_LVM_SEGMENT_SIZE (256 * 1024 * 1024)

/* 
 * Until we uncover the details of the segment descriptors format,
 * it is useless to try to do something more efficient.
 */

static int vmfs_lvm_get_extent_from_offset(vmfs_lvm_t *lvm,off_t pos)
{
   int extent;
   off_t segment = pos / VMFS_LVM_SEGMENT_SIZE;

   for (extent = 0; extent < lvm->loaded_extents; extent++) {
      if ((segment >= lvm->extents[extent]->vol_info.first_segment) &&
          (segment <= lvm->extents[extent]->vol_info.last_segment))
        return(extent);
   }

   return(-1);
}

/* Get extent size */
static inline ssize_t vmfs_lvm_extent_size(vmfs_lvm_t *lvm,int extent)
{
   return(lvm->extents[extent]->vol_info.num_segments * VMFS_LVM_SEGMENT_SIZE);
}

typedef ssize_t (*vmfs_vol_io_func)(vmfs_volume_t *,off_t,u_char *,size_t);

/* Read a raw block of data on logical volume */
static inline ssize_t vmfs_lvm_io(vmfs_lvm_t *lvm,off_t pos,u_char *buf,
                                  size_t len,vmfs_vol_io_func func)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (extent < 0)
      return(-1);

   pos -= lvm->extents[extent]->vol_info.first_segment * VMFS_LVM_SEGMENT_SIZE;
   if ((pos + len) > vmfs_lvm_extent_size(lvm,extent)) {
      /* TODO: Handle this case */
      fprintf(stderr,"VMFS: i/o spanned over several extents is unsupported\n");
      return(-1);
   }

   return(func(lvm->extents[extent],pos,buf,len));
}

/* Read a raw block of data on logical volume */
ssize_t vmfs_lvm_read(vmfs_lvm_t *lvm,off_t pos,u_char *buf,size_t len)
{
   return(vmfs_lvm_io(lvm,pos,buf,len,vmfs_vol_read));
}

/* Write a raw block of data on logical volume */
ssize_t vmfs_lvm_write(vmfs_lvm_t *lvm,off_t pos,u_char *buf,size_t len)
{
   return(vmfs_lvm_io(lvm,pos,buf,len,vmfs_vol_write));
}

/* Reserve the underlying volume given a LVM position */
int vmfs_lvm_reserve(vmfs_lvm_t *lvm,off_t pos)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (extent < 0)
      return(-1);

   return(vmfs_vol_reserve(lvm->extents[extent]));
}

/* Release the underlying volume given a LVM position */
int vmfs_lvm_release(vmfs_lvm_t *lvm,off_t pos)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (extent < 0)
      return(-1);

   return(vmfs_vol_release(lvm->extents[extent]));
}

/* Show lvm information */
void vmfs_lvm_show(vmfs_lvm_t *lvm) {
   char uuid_str[M_UUID_BUFLEN];
   int i;

   printf("Logical Volume Information:\n");
   printf("  - UUID    : %s\n",m_uuid_to_str(lvm->lvm_info.uuid,uuid_str));
   printf("  - Size    : %llu GB\n",
          lvm->extents[0]->vol_info.size / (1024*1048576));
   printf("  - Blocks  : %llu\n",lvm->extents[0]->vol_info.blocks);
   printf("  - Num. Extents : %u\n",lvm->extents[0]->vol_info.num_extents);

   printf("\n");

   for(i = 0; i < lvm->loaded_extents; i++) {
      vmfs_vol_show(lvm->extents[i]);
   }
}

/* Create a volume structure */
vmfs_lvm_t *vmfs_lvm_create(int debug_level)
{
   vmfs_lvm_t *lvm;

   if (!(lvm = calloc(1,sizeof(*lvm))))
      return NULL;

   lvm->debug_level = debug_level;
   return lvm;
}

/* Add an extent to the LVM */
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, char *filename)
{
   vmfs_volume_t *vol;

   if (!(vol = vmfs_vol_create(filename, lvm->debug_level)))
      return(-1);

   if (vmfs_vol_open(vol))
      return(-1);

   if (lvm->loaded_extents == 0) {
      uuid_copy(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid);
      lvm->lvm_info.size = vol->vol_info.size;
      lvm->lvm_info.blocks = vol->vol_info.blocks;
      lvm->lvm_info.num_extents = vol->vol_info.num_extents;
   } else if (uuid_compare(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid)) {
      fprintf(stderr, "VMFS: The %s file/device is not part of the LVM\n", filename);
      return(-1);
   } else if ((lvm->lvm_info.size != vol->vol_info.size) ||
              (lvm->lvm_info.blocks != vol->vol_info.blocks) ||
              (lvm->lvm_info.num_extents != vol->vol_info.num_extents)) {
      fprintf(stderr, "VMFS: LVM information mismatch for the %s"
                      " file/device\n", filename);
      return(-1);
   }

   lvm->extents[lvm->loaded_extents++] = vol;
   return(0);
}

/* Open an LVM */
int vmfs_lvm_open(vmfs_lvm_t *lvm)
{
   if (lvm->loaded_extents != lvm->lvm_info.num_extents) {
      fprintf(stderr, "VMFS: Missing extents\n");
      return(-1);
   }
   return(0);
}