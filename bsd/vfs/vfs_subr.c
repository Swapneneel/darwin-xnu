 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */
#include <sys/vnode.h>
#include <sys/uio_internal.h>
#include <sys/uio.h>
#include <kern/kalloc.h>	/* kalloc()/kfree() */
#include <kern/clock.h>		/* delay_for_interval() */
#include <libkern/OSAtomic.h>	/* OSAddAtomic() */


#include <vm/vm_protos.h>	/* vnode_pager_vrele() */

#if CONFIG_MACF
#include <security/mac_framework.h>
#endif

/* XXX next protptype should be from <nfs/nfs.h> */
extern int       nfs_vinvalbuf(vnode_t, int, vfs_context_t, int);

/* XXX next prototytype should be from libsa/stdlib.h> but conflicts libkern */
__private_extern__ void qsort(
    void * array,
    size_t nmembers,
    size_t member_size,
    int (*)(const void *, const void *));

__private_extern__ void vntblinit(void);
__private_extern__ kern_return_t reset_vmobjectcache(unsigned int val1,
			unsigned int val2);
__private_extern__ int unlink1(vfs_context_t, struct nameidata *, int);
static void vnode_list_remove_locked(vnode_t);
static void vgone(vnode_t, int flags);
static void vclean(vnode_t vp, int flag);
static void vnode_reclaim_internal(vnode_t, int, int, int);
static void vnode_dropiocount (vnode_t);
static errno_t vnode_getiocount(vnode_t vp, int vid, int vflags);
errno_t rmdir_remove_orphaned_appleDouble(vnode_t, vfs_context_t, int *); 

TAILQ_HEAD(deadlst, vnode) vnode_dead_list;	/* vnode dead list */

TAILQ_HEAD(ragelst, vnode) vnode_rage_list;	/* vnode rapid age list */
struct timeval rage_tv;
int	rage_limit = 0;
int	ragevnodes = 0;			   

#define RAGE_LIMIT_MIN	100
#define RAGE_TIME_LIMIT	5



/* remove a vnode from dead vnode list */
#define VREMDEAD(fun, vp)	\
		VLISTCHECK((fun), (vp), "dead");	\
		TAILQ_REMOVE(&vnode_dead_list, (vp), v_freelist);	\
		vp->v_listflag &= ~VLIST_DEAD;	\
		deadvnodes--;	\
	} while(0)


/* remove a vnode from rage vnode list */
#define VREMRAGE(fun, vp)	\
	do {	\
	        if ( !(vp->v_listflag & VLIST_RAGE))			\
		        panic("VREMRAGE: vp not on rage list");		\
		VLISTCHECK((fun), (vp), "rage");			\
		TAILQ_REMOVE(&vnode_rage_list, (vp), v_freelist);	\
		VLISTNONE((vp));		\
		vp->v_listflag &= ~VLIST_RAGE;	\
		ragevnodes--;			\
 * vnodetarget hasn't been used in a long time, but
 * it was exported for some reason... I'm leaving in
 * place for now...  it should be deprecated out of the
 * exports and removed eventually.
#define VNODE_FREE_MIN		CONFIG_VNODE_FREE_MIN	/* freelist should have at least this many */
	TAILQ_INIT(&vnode_rage_list);
	TAILQ_INIT(&vnode_dead_list);
	microuptime(&rage_tv);
	rage_limit = desiredvnodes / 100;

	if (rage_limit < RAGE_LIMIT_MIN)
	        rage_limit = RAGE_LIMIT_MIN;
	
vnode_waitforwrites(vnode_t vp, int output_target, int slpflag, int slptimeout, const char *msg) {

		if (vp->v_numoutput <= 1) {
		        int need_wakeup = 0;
		        vnode_lock_spin(vp);
			if (vp->v_numoutput < 0)
			        panic("vnode_writedone: numoutput < 0");

			if ((vp->v_flag & VTHROTTLED) && (vp->v_numoutput <= 1)) {
			        vp->v_flag &= ~VTHROTTLED;
				need_wakeup = 1;
			}
			if ((vp->v_flag & VBWAIT) && (vp->v_numoutput == 0)) {
			        vp->v_flag &= ~VBWAIT;
				need_wakeup = 1;
			}
			vnode_unlock(vp);
			if (need_wakeup)
			        wakeup((caddr_t)&vp->v_numoutput);
		}
		msleep((caddr_t)mp, &mp->mnt_mlock, PVFS, "vnode_iterate_setup", NULL);	
		/* disable preflight only for udf, a hack to be removed after 4073176 is fixed */
		if (vp->v_tag == VT_UDF)
			return 0;
		if (vp->v_type == VDIR)
			continue;
vnode_iterate(mount_t mp, int flags, int (*callout)(struct vnode *, void *),
	      void *arg)
		msleep((caddr_t)&mp->mnt_iterref, mnt_list_mtx_lock, PVFS, "mount_iterdrain", NULL);
		msleep((caddr_t)&mp->mnt_lflag, &mp->mnt_mlock, PVFS, "mount_drain", NULL);
			msleep((caddr_t)mp, &mp->mnt_mlock, (PVFS | PDROP), "vfsbusy", NULL);
#if CONFIG_MACF
	mac_mount_label_destroy(mp);
#endif

	mp->mnt_alignmentmask = PAGE_MASK;
	mp->mnt_ioflags = 0;
	mp->mnt_realrootvp = NULLVP;
	mp->mnt_authcache_ttl = CACHED_LOOKUP_RIGHT_TTL;
	/* XXX const poisoning layering violation */
	(void) copystr((const void *)devname, mp->mnt_vfsstat.f_mntfromname, MAXPATHLEN - 1, NULL);
#if CONFIG_MACF
	mac_mount_label_init(mp);
	mac_mount_label_associate(vfs_context_kernel(), mp);
#endif
	        if (!strncmp(vfsp->vfc_name, fstypename,
			     sizeof(vfsp->vfc_name)))
vfs_mountroot(void)
#if CONFIG_MACF
	struct vnode *vp;
#endif
	vfs_context_t ctx = vfs_context_kernel();
	struct vfs_attr	vfsattr;
	vnode_t	bdevvp_rootvp;
		/*
		error = (*mountroot)();
		printf("vfs_mountroot: can't setup bdevvp\n");
	/*
	 * 4951998 - code we call in vfc_mountroot may replace rootvp 
	 * so keep a local copy for some house keeping.
	 */
	bdevvp_rootvp = rootvp;
		if ((error = (*vfsp->vfc_mountroot)(mp, rootvp, ctx)) == 0) {
			if ( bdevvp_rootvp != rootvp ) {
				/*
				 * rootvp changed...
				 *   bump the iocount and fix up mnt_devvp for the
				 *   new rootvp (it will already have a usecount taken)...
				 *   drop the iocount and the usecount on the orignal
				 *   since we are no longer going to use it...
				 */
				vnode_getwithref(rootvp);
				mp->mnt_devvp = rootvp;

			        vnode_rele(bdevvp_rootvp);
			        vnode_put(bdevvp_rootvp);
			}
			mp->mnt_devvp->v_specflags |= SI_MOUNTEDON;
			vfs_unbusy(mp);

			/*
			 * Shadow the VFC_VFSNATIVEXATTR flag to MNTK_EXTENDED_ATTRS.
			 */
			if (mp->mnt_vtable->vfc_vfsflags & VFC_VFSNATIVEXATTR) {
				mp->mnt_kern_flag |= MNTK_EXTENDED_ATTRS;
			}
			if (mp->mnt_vtable->vfc_vfsflags & VFC_VFSPREFLIGHT) {
				mp->mnt_kern_flag |= MNTK_UNMOUNT_PREFLIGHT;
			}

			/*
			 * Probe root file system for additional features.
			 */
			(void)VFS_START(mp, 0, ctx);

			VFSATTR_INIT(&vfsattr);
			VFSATTR_WANTED(&vfsattr, f_capabilities);
			if (vfs_getattr(mp, &vfsattr, ctx) == 0 && 
			    VFSATTR_IS_SUPPORTED(&vfsattr, f_capabilities)) {
				if ((vfsattr.f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_EXTENDED_ATTR) &&
				    (vfsattr.f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_EXTENDED_ATTR)) {
					mp->mnt_kern_flag |= MNTK_EXTENDED_ATTRS;
				}
#if NAMEDSTREAMS
				if ((vfsattr.f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_NAMEDSTREAMS) &&
				    (vfsattr.f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] & VOL_CAP_INT_NAMEDSTREAMS)) {
					mp->mnt_kern_flag |= MNTK_NAMED_STREAMS;
				}
#endif
				if ((vfsattr.f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_PATH_FROM_ID) &&
				    (vfsattr.f_capabilities.valid[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_PATH_FROM_ID)) {
					mp->mnt_kern_flag |= MNTK_PATH_FROM_ID;
				}
			}

			 * by bdevvp (or picked up by us on the substitued
			 * rootvp)... it (or we) will have also taken
#if CONFIG_MACF
			if ((vfs_flags(mp) & MNT_MULTILABEL) == 0)
				return (0);

			error = VFS_ROOT(mp, &vp, ctx);
			if (error) {
				printf("%s() VFS_ROOT() returned %d\n",
				    __func__, error);
				dounmount(mp, MNT_FORCE, 0, ctx);
				goto fail;
			}

			/* VFS_ROOT provides reference so flags = 0 */
			error = vnode_label(mp, NULL, vp, NULL, 0, ctx);
			if (error) {
				printf("%s() vnode_label() returned %d\n",
				    __func__, error);
				dounmount(mp, MNT_FORCE, 0, ctx);
				goto fail;
			}
#endif
#if CONFIG_MACF
fail:
#endif
vfs_getvfs(fsid_t *fsid)
vfs_getvfs_locked(fsid_t *fsid)
vfs_getvfs_by_mntonname(char *path)
		if (!strncmp(mp->mnt_vfsstat.f_mntonname, path,
					sizeof(mp->mnt_vfsstat.f_mntonname))) {
vfs_getnewfsid(struct mount *mp)
long numvnodes, freevnodes, deadvnodes;
		vp->v_mntvnodes.tqe_next = NULL;
		vp->v_mntvnodes.tqe_prev = NULL;
	context.vc_thread = current_thread();
	vfsp.vnfs_dvp = NULL;
	vfsp.vnfs_fsnode = NULL;
	vfsp.vnfs_cnp = NULL;
	vnode_lock_spin(nvp);
	nvp->v_flag |= VBDEVVP;
	nvp->v_tag = VT_NON;	/* set this to VT_NON so during aliasing it can be replaced */
	vnode_unlock(nvp);

#if CONFIG_MACF
	/* 
	 * XXXMAC: We can't put a MAC check here, the system will
	 * panic without this vnode.
	 */
#endif /* MAC */	

checkalias(struct vnode *nvp, dev_t nvp_rdev)
		        vnode_reclaim_internal(vp, 1, 1, 0);
			vnode_put_locked(vp);
retnullvp:
			vnode_put_locked(vp);
	if ((vp->v_flag & (VBDEVVP | VDEVFLUSH)) != 0)
		return(vp);
	else  {
		panic("checkalias with VT_NON vp that shouldn't: %x", (unsigned int)vp);
		goto retnullvp;
	}
	int vpid;
	vnode_lock_spin(vp);
	        error = vnode_getiocount(vp, vpid, vflags);
/*
 * Returns:	0			Success
 *		ENOENT			No such file or directory [terminating]
 */
/*
 * Returns:	0			Success
 *		ENOENT			No such file or directory [terminating]
 */
	vnode_lock_spin(vp);
		panic("vnode_ref_ext: vp %p has no valid reference %d, %d", vp, vp->v_iocount, vp->v_usecount);
	if (vp->v_flag & VRAGE) {
	        struct  uthread *ut;

	        ut = get_bsdthread_info(current_thread());
		
	        if ( !(current_proc()->p_lflag & P_LRAGE_VNODES) &&
		     !(ut->uu_flag & UT_RAGE_VNODES)) {
		        /*
			 * a 'normal' process accessed this vnode
			 * so make sure its no longer marked
			 * for rapid aging...  also, make sure
			 * it gets removed from the rage list...
			 * when v_usecount drops back to 0, it
			 * will be put back on the real free list
			 */
			vp->v_flag &= ~VRAGE;
			vp->v_references = 0;
			vnode_list_remove(vp);
		}
	}
	if ((vp->v_flag & VRAGE) && !(vp->v_lflag & VL_DEAD)) {
		/*
		 * add the new guy to the appropriate end of the RAGE list
		 */
		if ((vp->v_flag & VAGE))
		        TAILQ_INSERT_HEAD(&vnode_rage_list, vp, v_freelist);
		else
		        TAILQ_INSERT_TAIL(&vnode_rage_list, vp, v_freelist);

		vp->v_listflag |= VLIST_RAGE;
		ragevnodes++;

		/*
		 * reset the timestamp for the last inserted vp on the RAGE
		 * queue to let new_vnode know that its not ok to start stealing
		 * from this list... as long as we're actively adding to this list
		 * we'll push out the vnodes we want to donate to the real free list
		 * once we stop pushing, we'll let some time elapse before we start
		 * stealing them in the new_vnode routine
		 */
		microuptime(&rage_tv);
	        /*
		 * if VL_DEAD, insert it at head of the dead list
		 * else insert at tail of LRU list or at head if VAGE is set
		 */
	        if ( (vp->v_lflag & VL_DEAD)) {
		        TAILQ_INSERT_HEAD(&vnode_dead_list, vp, v_freelist);
			vp->v_listflag |= VLIST_DEAD;
			deadvnodes++;
		} else if ((vp->v_flag & VAGE)) {
		        TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
			vp->v_flag &= ~VAGE;
			freevnodes++;
		} else {
		        TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
			freevnodes++;
		}

 * called with vnode LOCKED and
 * the list lock held
 */
static void
vnode_list_remove_locked(vnode_t vp)
{
	if (VONLIST(vp)) {
		/*
		 * the v_listflag field is
		 * protected by the vnode_list_lock
		 */
	        if (vp->v_listflag & VLIST_RAGE)
		        VREMRAGE("vnode_list_remove", vp);
		else if (vp->v_listflag & VLIST_DEAD)
		        VREMDEAD("vnode_list_remove", vp);
		else
		        VREMFREE("vnode_list_remove", vp);
	}
}


/*
 * remove the vnode from appropriate free list.
 * called with vnode LOCKED
		 * to the not-on-list state until we
		 * is due to "new_vnode" removing vnodes
		vnode_list_remove_locked(vp);

	        vnode_lock_spin(vp);
		panic("vnode_rele_ext: vp %p usecount -ve : %d", vp,  vp->v_usecount);
		        panic("vnode_rele_ext: vp %p writecount -ve : %ld", vp,  vp->v_writecount);
		        panic("vnode_rele_ext: vp %p kusecount -ve : %d", vp,  vp->v_kusecount);
	if (vp->v_kusecount > vp->v_usecount)
		panic("vnode_rele_ext: vp %p kusecount(%d) out of balance with usecount(%d)\n",vp, vp->v_kusecount, vp->v_usecount);
			vp->v_flag  &= ~(VNOCACHE_DATA | VRAOFF | VOPENEVT);
	vp->v_flag  &= ~(VNOCACHE_DATA | VRAOFF | VOPENEVT);
	VNOP_INACTIVE(vp, vfs_context_current());
	vnode_lock_spin(vp);
		vnode_lock_convert(vp);
	        vnode_reclaim_internal(vp, 1, 1, 0);
	vnode_dropiocount(vp);
vflush(struct mount *mp, struct vnode *skipvp, int flags)
	int retval;
	int vid;
	if (((flags & FORCECLOSE)==0)  && ((mp->mnt_kern_flag & MNTK_UNMOUNT_PREFLIGHT) != 0)) {
			vnode_reclaim_internal(vp, 1, 1, 0);
			vnode_dropiocount(vp);

				vnode_reclaim_internal(vp, 1, 1, 0);
				vnode_dropiocount(vp);
				vclean(vp, 0);
				vp->v_flag |= VDEVFLUSH;
long num_recycledvnodes = 0;	/* long for OSAddAtomic */
vclean(vnode_t vp, int flags)
	vfs_context_t ctx = vfs_context_current();
	int clflags = 0;
#if NAMEDSTREAMS
	int is_namedstream;
#endif
#if NAMEDSTREAMS
	is_namedstream = vnode_isnamedstream(vp);
#endif
	if (flags & DOCLOSE)
		clflags |= IO_NDELAY;
	if (flags & REVOKEALL)
		clflags |= IO_REVOKE;
	
		VNOP_CLOSE(vp, clflags, ctx);
			nfs_vinvalbuf(vp, V_SAVE, ctx, 0);
		        VNOP_FSYNC(vp, MNT_WAIT, ctx);
		VNOP_INACTIVE(vp, ctx);
#if NAMEDSTREAMS
	/* Delete the shadow stream file before we reclaim its vnode */
	if ((is_namedstream != 0) &&
			(vp->v_parent != NULLVP) &&
			((vp->v_parent->v_mount->mnt_kern_flag & MNTK_NAMED_STREAMS) == 0)) {
		vnode_relenamedstream(vp->v_parent, vp, ctx);
	}
#endif

	/*
	 * Destroy ubc named reference
	 * cluster_release is done on this path
	 * along with dropping the reference on the ucred
	 */
	if (VNOP_RECLAIM(vp, ctx))
#if DIAGNOSTIC
#else
vn_revoke(vnode_t vp, __unused int flags, __unused vfs_context_t a_context)
#endif
				vnode_reclaim_internal(vq, 0, 1, 0);
	vnode_reclaim_internal(vp, 0, 0, REVOKEALL);
vnode_recycle(struct vnode *vp)
	vnode_reclaim_internal(vp, 1, 0, 0);

	vnode_lock_spin(vp);
vgone(vnode_t vp, int flags)
	vclean(vp, flags | DOCLOSE);
		vnode_lock_spin(vp);
	count = 0;
	/*
	 * Grab first vnode and its vid.
	 */
	vq = *vp->v_hashchain;
	vid = vq ? vq->v_id : 0;

	SPECHASH_UNLOCK();
	while (vq) {
		/*
		 * Attempt to get the vnode outside the SPECHASH lock.
		 */

		if (vq->v_rdev == vp->v_rdev && vq->v_type == vp->v_type) {
			if ((vq->v_usecount == 0) && (vq->v_iocount == 1)  && vq != vp) {
				/*
				 * Alias, but not in use, so flush it out.
				 */
				vnode_reclaim_internal(vq, 1, 1, 0);
				vnode_put_locked(vq);
				vnode_unlock(vq);
				goto loop;
			}
			count += (vq->v_usecount - vq->v_kusecount);
		/*
		 * must do this with the reference still held on 'vq'
		 * so that it can't be destroyed while we're poking
		 * through v_specnext
		 */
		vnext = vq->v_specnext;
		vid = vnext ? vnext->v_id : 0;

		SPECHASH_UNLOCK();

		vnode_put(vq);

		vq = vnext;
#if !CONFIG_NO_PRINTF_STRINGS
static const char *typename[] =
#endif
	printf("type %s, usecount %d, writecount %ld",
		strlcat(sbuf, "|VROOT", sizeof(sbuf));
		strlcat(sbuf, "|VTEXT", sizeof(sbuf));
		strlcat(sbuf, "|VSYSTEM", sizeof(sbuf));
		strlcat(sbuf, "|VNOFLUSH", sizeof(sbuf));
		strlcat(sbuf, "|VBWAIT", sizeof(sbuf));
		strlcat(sbuf, "|VALIASED", sizeof(sbuf));
	return build_path(vp, pathbuf, *len, len, BUILDPATH_NO_FS_ENTER, vfs_context_current());
}


int
vn_getcdhash(struct vnode *vp, off_t offset, unsigned char *cdhash)
{
	return ubc_cs_getcdhash(vp, offset, cdhash);
extension_cmp(const void *a, const void *b)
    return (strlen((const char *)a) - strlen((const char *)b));
    char *new_exts;
    int error;
is_package_name(const char *name, int len)
    const char *ptr, *name_ext;
           user_addr_t newp, size_t newlen, proc_t p)
	/* All non VFS_GENERIC and in VFS_GENERIC, 
	 * VFS_MAXTYPENUM, VFS_CONF, VFS_SET_PACKAGE_EXTS
	 * needs to have root priv to have modifiers. 
	 * For rest the userland_sysctl(CTLFLAG_ANYBODY) would cover.
	 */
	if ((newp != USER_ADDR_NULL) && ((name[0] != VFS_GENERIC) || 
			((name[1] == VFS_MAXTYPENUM) ||
			 (name[1] == VFS_CONF) ||
			 (name[1] == VFS_SET_PACKAGE_EXTS)))
	     && (error = suser(kauth_cred_get(), &p->p_acflag))) {
			return(error);
	}
		/* XXX current context proxy for proc p? */
		            oldp, oldlenp, newp, newlen,
			    vfs_context_current()));
 * Dump vnode list (via sysctl) - defunct
 * use "pstat" instead
sysctl_vnode
(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, __unused struct sysctl_req *req)
SYSCTL_PROC(_kern, KERN_VNODE, vnode,
		CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_MASKED,
		0, 0, sysctl_vnode, "S,", "");


vfs_mountedon(struct vnode *vp)
vfs_unmountall(void)
		error = dounmount(mp, MNT_FORCE, 0, vfs_context_current());
		if ((error != 0) && (error != EBUSY)) {
			printf("unmount of %s failed (", mp->mnt_vfsstat.f_mntonname);
			printf("%d)\n", error);
		} else if (error == EBUSY) {
			/* If EBUSY is returned,  the unmount was already in progress */
			printf("unmount of %x failed (", (unsigned int)mp);
			printf("BUSY)\n");	
		} 
 * This routine is called from vnode_pager_deallocate out of the VM
 * The path to vnode_pager_deallocate can only be initiated by ubc_destroy_named
 * on a vnode that has a UBCINFO
vnode_pager_vrele(vnode_t vp)
        struct ubc_info *uip;

	uip = vp->v_ubcinfo;
	vp->v_ubcinfo = UBC_INFO_NULL;
	ubc_info_deallocate(uip);
	off_t	alignment;
	u_int32_t features;
	vfs_context_t ctx = vfs_context_current();
	        if (VNOP_IOCTL(rootvp, DKIOCGETBSDUNIT, (caddr_t)&rootunit, 0, ctx))
	        if (VNOP_IOCTL(devvp, DKIOCGETBSDUNIT, (caddr_t)&thisunit, 0, ctx) == 0) {
				(caddr_t)&blksize, 0, ctx)))
	if (VNOP_IOCTL(devvp, DKIOCISVIRTUAL, (caddr_t)&isvirtual, 0, ctx) == 0) {
	if ((error = VNOP_IOCTL(devvp, DKIOCGETFEATURES,
				(caddr_t)&features, 0, ctx)))
		return (error);

				(caddr_t)&readblockcnt, 0, ctx)))
				(caddr_t)&writeblockcnt, 0, ctx)))
				(caddr_t)&readmaxcnt, 0, ctx)))
				(caddr_t)&writemaxcnt, 0, ctx)))
				(caddr_t)&readsegcnt, 0, ctx)))
				(caddr_t)&writesegcnt, 0, ctx)))
				(caddr_t)&readsegsize, 0, ctx)))
				(caddr_t)&writesegsize, 0, ctx)))
		return (error);

	if ((error = VNOP_IOCTL(devvp, DKIOCGETMINSEGMENTALIGNMENTBYTECOUNT,
				(caddr_t)&alignment, 0, ctx)))
	if (alignment)
	        temp = (alignment > PAGE_SIZE) ? PAGE_MASK : alignment - 1;
	else
	        temp = 0;
	mp->mnt_alignmentmask = temp;

	if (features & DK_FEATURE_FORCE_UNIT_ACCESS)
	        mp->mnt_ioflags |= MNT_IOFLAGS_FUA_SUPPORTED;
	
lck_grp_t *fs_klist_lck_grp;
lck_mtx_t *fs_klist_lock;
	fs_klist_lck_grp = lck_grp_alloc_init("fs_klist", NULL);
	fs_klist_lock = lck_mtx_alloc_init(fs_klist_lck_grp, NULL);
	lck_mtx_lock(fs_klist_lock);
	lck_mtx_unlock(fs_klist_lock);
sysctl_vfs_vfslist(__unused struct sysctl_oid *oidp, __unused void *arg1,
		__unused int arg2, struct sysctl_req *req)
sysctl_vfs_ctlbyfsid(__unused struct sysctl_oid *oidp, void *arg1, int arg2,
		struct sysctl_req *req)
	int *name, flags, namelen;
	int error=0, gotref=0;
	vfs_context_t ctx = vfs_context_current();
	proc_t p = req->p;	/* XXX req->p != current_proc()? */
			goto out;
		if (user_vc.vc_vers != VFS_CTL_VERS1) {
			error = EINVAL;
			goto out;
		}
		mp = mount_list_lookupby_fsid(&user_vc.vc_fsid, 0, 1);
			goto out;
		if (vc.vc_vers != VFS_CTL_VERS1) {
			error = EINVAL;
			goto out;
		}
		mp = mount_list_lookupby_fsid(&vc.vc_fsid, 0, 1);
	if (mp == NULL) {
		error = ENOENT;
		goto out;
	}
	gotref = 1;
				    ctx);
			    ctx);
		}
		if (error != ENOTSUP) {
			goto out;

		mount_ref(mp, 0);
		mount_iterdrop(mp);
		gotref = 0;
		/* safedounmount consumes a ref */
		error = safedounmount(mp, flags, ctx);
		    (error = vfs_update_vfsstat(mp, ctx, VFS_USER_EVENT)))
			goto out;
			strlcpy(sfs.f_fstypename, sp->f_fstypename, MFSNAMELEN);
			strlcpy(sfs.f_mntonname, sp->f_mntonname, MNAMELEN);
			strlcpy(sfs.f_mntfromname, sp->f_mntfromname, MNAMELEN);
			strlcpy(sfs.f_fstypename, sp->f_fstypename, MFSNAMELEN);
			strlcpy(sfs.f_mntonname, sp->f_mntonname, MNAMELEN);
			strlcpy(sfs.f_mntfromname, sp->f_mntfromname, MNAMELEN);
		error = ENOTSUP;
		goto out;
out:
	if(gotref != 0)
		mount_iterdrop(mp);
	lck_mtx_lock(fs_klist_lock);
	lck_mtx_unlock(fs_klist_lock);
	lck_mtx_lock(fs_klist_lock);
	lck_mtx_unlock(fs_klist_lock);
	/*
	 * Backwards compatibility:
	 * Other filters would do nothing if kn->kn_sfflags == 0
	 */

	if ((kn->kn_sfflags == 0) || (kn->kn_sfflags & hint)) {
		kn->kn_fflags |= hint;
	}
sysctl_vfs_noremotehang(__unused struct sysctl_oid *oidp,
		__unused void *arg1, __unused int arg2, struct sysctl_req *req)
	proc_t p;
	p = proc_find(pid < 0 ? -pid : pid);
		proc_rele(p);
	if (p != req->p && proc_suser(req->p) != 0) {
		proc_rele(p);
	}
		OSBitAndAtomic(~((uint32_t)P_NOREMOTEHANG), (UInt32 *)&p->p_flag);
		OSBitOrAtomic(P_NOREMOTEHANG, (UInt32 *)&p->p_flag);
	proc_rele(p);

SYSCTL_NODE(_vfs, VFS_GENERIC, generic, CTLFLAG_RW|CTLFLAG_LOCKED, NULL, "vfs generic hinge");
    NULL, 0, sysctl_vfs_vfslist, "S,fsid", "List of mounted filesystem ids");
SYSCTL_NODE(_vfs_generic, OID_AUTO, ctlbyfsid, CTLFLAG_RW|CTLFLAG_LOCKED,
SYSCTL_PROC(_vfs_generic, OID_AUTO, noremotehang, CTLFLAG_RW|CTLFLAG_ANYBODY,
    NULL, 0, sysctl_vfs_noremotehang, "I", "noremotehang");
long num_reusedvnodes = 0;	/* long for OSAddAtomic */
	int force_alloc = 0, walk_count = 0;
        struct timeval current_tv;
        struct unsafe_fsnode *l_unsafefs = 0;
	proc_t  curproc = current_proc();
	pid_t current_pid = proc_pid(curproc);
	microuptime(&current_tv);

	vp = NULLVP;

	if ( !TAILQ_EMPTY(&vnode_dead_list)) {
		 * Can always reuse a dead one
	        vp = TAILQ_FIRST(&vnode_dead_list);
		goto steal_this_vp;
	}
	 * no dead vnodes available... if we're under
	 * the limit, we'll create a new vnode
	if (numvnodes < desiredvnodes || force_alloc) {
		MALLOC_ZONE(vp, struct vnode *, sizeof(*vp), M_VNODE, M_WAITOK);
		bzero((char *)vp, sizeof(*vp));
#if CONFIG_MACF
		mac_vnode_label_init(vp);
#endif /* MAC */

		vp->v_iocount = 1;

#define MAX_WALK_COUNT 1000

	if ( !TAILQ_EMPTY(&vnode_rage_list) &&
	     (ragevnodes >= rage_limit ||
	      (current_tv.tv_sec - rage_tv.tv_sec) >= RAGE_TIME_LIMIT)) {

		TAILQ_FOREACH(vp, &vnode_rage_list, v_freelist) {
		    if ( !(vp->v_listflag & VLIST_RAGE) || !(vp->v_flag & VRAGE))
		        panic("new_vnode: vp on RAGE list not marked both VLIST_RAGE and VRAGE");

		    // skip vnodes which have a dependency on this process
		    // (i.e. they're vnodes in a disk image and this process
		    // is diskimages-helper)
		    //
		    if (vp->v_mount && vp->v_mount->mnt_dependent_pid != current_pid && vp->v_mount->mnt_dependent_process != curproc) {
			break;
		    }

		    // don't iterate more than MAX_WALK_COUNT vnodes to
		    // avoid keeping the vnode list lock held for too long.
		    if (walk_count++ > MAX_WALK_COUNT) {
			vp = NULL;
			break;
		    }
		}

	}

	if (vp == NULL && !TAILQ_EMPTY(&vnode_free_list)) {
	        /*
		 * Pick the first vp for possible reuse
		 */
		walk_count = 0;
		TAILQ_FOREACH(vp, &vnode_free_list, v_freelist) {
		    // skip vnodes which have a dependency on this process
		    // (i.e. they're vnodes in a disk image and this process
		    // is diskimages-helper)
		    //
		    if (vp->v_mount && vp->v_mount->mnt_dependent_pid != current_pid && vp->v_mount->mnt_dependent_process != curproc) {
			break;
		    }

		    // don't iterate more than MAX_WALK_COUNT vnodes to
		    // avoid keeping the vnode list lock held for too long.
		    if (walk_count++ > MAX_WALK_COUNT) {
			vp = NULL;
			break;
		    }
		}

	}

	//
	// if we don't have a vnode and the walk_count is >= MAX_WALK_COUNT
	// then we're trying to create a vnode on behalf of a
	// process like diskimages-helper that has file systems
	// mounted on top of itself (and thus we can't reclaim
	// vnodes in the file systems on top of us).  if we can't
	// find a vnode to reclaim then we'll just have to force
	// the allocation.
	//
	if (vp == NULL && walk_count >= MAX_WALK_COUNT) {
	    force_alloc = 1;
	    vnode_list_unlock();
	    goto retry;
	}

			delay_for_interval(1, 1000 * 1000);
			"%d free, %d dead, %d rage\n",
		        desiredvnodes, numvnodes, freevnodes, deadvnodes, ragevnodes);
		*vpp = NULL;
	vnode_list_remove_locked(vp);
	vnode_lock_spin(vp);
		vnode_lock_convert(vp);
		(void)vnode_reclaim_internal(vp, 1, 1, 0);
	        l_unsafefs = vp->v_unsafefs;

#if CONFIG_MACF
	/*
	 * We should never see VL_LABELWAIT or VL_LABEL here.
	 * as those operations hold a reference.
	 */
	assert ((vp->v_lflag & VL_LABELWAIT) != VL_LABELWAIT);
	assert ((vp->v_lflag & VL_LABEL) != VL_LABEL);
	if (vp->v_lflag & VL_LABELED) {
	        vnode_lock_convert(vp);
		mac_vnode_label_recycle(vp);
	}
#endif /* MAC */

	vp->v_iocount = 1;
	vp->v_mount = NULL;

	if (l_unsafefs) {
	        lck_mtx_destroy(&l_unsafefs->fsnodelock, vnode_lck_grp);
		FREE_ZONE((void *)l_unsafefs, sizeof(struct unsafe_fsnode), M_UNSAFEFS);
	}
void
vnode_lock_spin(vnode_t vp)
{
	lck_mtx_lock_spin(&vp->v_lock);
}

	lck_mtx_unlock(&vp->v_lock);
}



int
vnode_get(struct vnode *vp)
{
        int retval;
        vnode_lock_spin(vp);
	retval = vnode_get_locked(vp);
	vnode_unlock(vp);
	return(retval);	
}
vnode_get_locked(struct vnode *vp)
	if ((vp->v_iocount == 0) && (vp->v_lflag & (VL_TERMINATE | VL_DEAD))) {
	return (0);
	vnode_lock_spin(vp);
	vfs_context_t ctx = vfs_context_current();	/* hoist outside loop */
		panic("vnode_put(%p): iocount < 1", vp);
		vnode_dropiocount(vp);
		VNOP_INACTIVE(vp, ctx);
		vnode_lock_spin(vp);
	if ((vp->v_lflag & (VL_MARKTERM | VL_TERMINATE | VL_DEAD)) == VL_MARKTERM) {
	        vnode_lock_convert(vp);
	        vnode_reclaim_internal(vp, 1, 1, 0);
	}
	vnode_dropiocount(vp);
		vnode_lock_spin(vp);
	if ((vp->v_type != VREG) && ((vp->v_usecount - vp->v_kusecount) >  refcnt)) {
	vnode_lock_spin(vp);
		vp->v_owner = NULL;
/* suspend vnode_t
 * Please do not use on more than one vnode at a time as it may
 * cause deadlocks.
 * xxx should we explicity prevent this from happening?
 */

errno_t
vnode_suspend(vnode_t vp)
{
	if (vp->v_lflag & VL_SUSPENDED) {
		return(EBUSY);
	}

	vnode_lock_spin(vp);

	/* 
	 * xxx is this sufficient to check if a vnode_drain is 
	 * progress?
	 */

	if (vp->v_owner == NULL) {
		vp->v_lflag |= VL_SUSPENDED;
		vp->v_owner = current_thread();
	}
	vnode_unlock(vp);

	return(0);
}
					
					

		msleep(&vp->v_iocount, &vp->v_lock, PVFS, "vnode_drain", NULL);
#define UNAGE_THRESHHOLD	25
static errno_t
vnode_getiocount(vnode_t vp, int vid, int vflags)
	        if (nodead && (vp->v_lflag & VL_DEAD) && ((vp->v_type == VBAD) || (vp->v_data == 0))) {
		vnode_lock_convert(vp);

			msleep(&vp->v_lflag,   &vp->v_lock, PVFS, "vnode getiocount", NULL);
			msleep(&vp->v_iocount, &vp->v_lock, PVFS, "vnode_getiocount", NULL);
vnode_dropiocount (vnode_t vp)
		panic("vnode_dropiocount(%p): v_iocount < 1", vp);
	if ((vp->v_lflag & (VL_DRAIN | VL_SUSPENDED)) && (vp->v_iocount <= 1)) {
	        vnode_lock_convert(vp);
	}
	vnode_reclaim_internal(vp, 0, 0, 0);
vnode_reclaim_internal(struct vnode * vp, int locked, int reuse, int flags)
	vn_clearunionwait(vp, 1);

		vgone(vp, flags);		/* clean and reclaim the vnode */
	 * give the vnode a new identity so that vnode_getwithvid will fail
	 * on any stale cache accesses...
	 * grab the list_lock so that if we're in "new_vnode"
	 * behind the list_lock trying to steal this vnode, the v_id is stable...
	 * once new_vnode drops the list_lock, it will block trying to take
	 * the vnode lock until we release it... at that point it will evaluate
	 * whether the v_vid has changed
	 * also need to make sure that the vnode isn't on a list where "new_vnode"
	 * can find it after the v_id has been bumped until we are completely done
	 * with the vnode (i.e. putting it back on a list has to be the very last
	 * thing we do to this vnode... many of the callers of vnode_reclaim_internal
	 * are holding an io_count on the vnode... they need to drop the io_count
	 * BEFORE doing a vnode_list_add or make sure to hold the vnode lock until
	 * they are completely done with the vnode
	vnode_list_lock();

	vnode_list_remove_locked(vp);

	vnode_list_unlock();

		panic("vnode_reclaim_internal: clean vnode has pending I/O's");
	vp->v_socket = NULL;
	vp->v_owner = NULL;
	if (!reuse) {
	        /*
		 * make sure we get on the
		 * dead list if appropriate
		 */
	}
        struct  uthread *ut;
			if (vp->v_type == VREG) {
					vp->v_mount = NULL;
				vp->v_tag = VT_DEVFS;		/* callers will reset if needed (bdevvp) */

					vclean(vp, 0);
			/* Add fs named reference. */
			if (param->vnfs_flags & VNFS_ADDFSREF) {
				vp->v_lflag |= VNAMED_FSHASH;
			}
				if ((cnp->cn_flags & UNIONCREATED) == UNIONCREATED)
					vp->v_flag |= VISUNION;
			ut = get_bsdthread_info(current_thread());
		
			if ((current_proc()->p_lflag & P_LRAGE_VNODES) ||
			    (ut->uu_flag & UT_RAGE_VNODES)) {
			        /*
				 * process has indicated that it wants any
				 * vnodes created on its behalf to be rapidly
				 * aged to reduce the impact on the cached set
				 * of vnodes
				 */
			        vp->v_flag |= VRAGE;
			}
	vnode_lock_spin(vp);
	vnode_lock_spin(vp);
 * MAC: Parameter eventtype added, indicating whether the event that
 * triggered this update came from user space, via a system call
 * (VFS_USER_EVENT) or an internal kernel call (VFS_KERNEL_EVENT).
vfs_update_vfsstat(mount_t mp, vfs_context_t ctx, __unused int eventtype)
#if CONFIG_MACF
	if (eventtype == VFS_USER_EVENT) {
		error = mac_mount_check_getattr(ctx, mp, &va);
		if (error != 0)
			return (error);
	}
#endif

		/* 4822056 - protect against malformed server mount */
		mp->mnt_vfsstat.f_bsize = (va.f_bsize > 0 ? va.f_bsize : 512);
	mp->mnt_list.tqe_next = NULL;
	mp->mnt_list.tqe_prev = NULL;
#if CONFIG_VOLFS
	mount_t mp;
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		if (!(mp->mnt_kern_flag & MNTK_UNMOUNT) &&
		    (mp->mnt_kern_flag & MNTK_PATH_FROM_ID) &&
		    (mp->mnt_vfsstat.f_fsid.val[0] == volfs_id)) {
			cur_mount = mp;
			break;
		}
		}
#endif
mount_list_lookupby_fsid(fsid_t *fsid, int locked, int withref)
vnode_lookup(const char *path, int flags, vnode_t *vpp, vfs_context_t ctx)
	if (ctx == NULL) {		/* XXX technically an error */
		ctx = vfs_context_current();
vnode_open(const char *path, int fmode, int cmode, int flags, vnode_t *vpp, vfs_context_t ctx)
	if (ctx == NULL) {		/* XXX technically an error */
		ctx = vfs_context_current();
vnode_close(vnode_t vp, int flags, vfs_context_t ctx)
	if (ctx == NULL) {
		ctx = vfs_context_current();
	error = vn_close(vp, flags, ctx);
/*
 * Returns:	0			Success
 *	vnode_getattr:???
 */
/*
 * Create a filesystem object of arbitrary type with arbitrary attributes in
 * the spevied directory with the specified name.
 *
 * Parameters:	dvp			Pointer to the vnode of the directory
 *					in which to create the object.
 *		vpp			Pointer to the area into which to
 *					return the vnode of the created object.
 *		cnp			Component name pointer from the namei
 *					data structure, containing the name to
 *					use for the create object.
 *		vap			Pointer to the vnode_attr structure
 *					describing the object to be created,
 *					including the type of object.
 *		flags			VN_* flags controlling ACL inheritance
 *					and whether or not authorization is to
 *					be required for the operation.
 *		
 * Returns:	0			Success
 *		!0			errno value
 *
 * Implicit:	*vpp			Contains the vnode of the object that
 *					was created, if successful.
 *		*cnp			May be modified by the underlying VFS.
 *		*vap			May be modified by the underlying VFS.
 *					modified by either ACL inheritance or
 *		
 *		
 *					be modified, even if the operation is
 *					
 *
 * Notes:	The kauth_filesec_t in 'vap', if any, is in host byte order.
 *
 *		Modification of '*cnp' and '*vap' by the underlying VFS is
 *		strongly discouraged.
 *
 * XXX:		This function is a 'vn_*' function; it belongs in vfs_vnops.c
 *
 * XXX:		We should enummerate the possible errno values here, and where
 *		in the code they originated.
 */
#if CONFIG_MACF
	if (!(flags & VN_CREATE_NOLABEL)) {
		error = vnode_label(vnode_mount(vp), dvp, vp, cnp,
		    VNODE_LABEL_CREATE|VNODE_LABEL_NEEDREF, ctx);
		if (error)
			goto error;
	}
#endif

#if CONFIG_MACF
error:
#endif
static int	vnode_authorize_callback(kauth_cred_t credential, void *idata, kauth_action_t action,
    uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
static int	vnode_authorize_callback_int(__unused kauth_cred_t credential, __unused void *idata, kauth_action_t action,
 *
 * Returns:	0			Success
 *	kauth_authorize_action:EPERM	...
 *	xlate => EACCES			Permission denied
 *	kauth_authorize_action:0	Success
 *	kauth_authorize_action:		Depends on callback return; this is
 *					usually only vnode_authorize_callback(),
 *					but may include other listerners, if any
 *					exist.
 *		EROFS
 *		EACCES
 *		EPERM
 *		???
vnode_authorize(vnode_t vp, vnode_t dvp, kauth_action_t action, vfs_context_t ctx)
	result = kauth_authorize_action(vnode_scope, vfs_context_ucred(ctx), action,
		   (uintptr_t)ctx, (uintptr_t)vp, (uintptr_t)dvp, (uintptr_t)&error);
	        return(error);
	const char *where = "uninitialized";
vnode_authorize_simple(vauth_ctx vcp, kauth_ace_rights_t acl_rights, kauth_ace_rights_t preauth_rights, boolean_t *found_deny)
		*found_deny = eval.ae_found_deny;

		mp = vp->v_mount;
 * Handle authorization actions for filesystems that advertise that the
 * server will be enforcing.
 *
 * Returns:	0			Authorization should be handled locally
 *		1			Authorization was handled by the FS
 *
 * Note:	Imputed returns will only occur if the authorization request
 *		was handled by the FS.
 *
 * Imputed:	*resultp, modified	Return code from FS when the request is
 *					handled by the FS.
 *		VNOP_ACCESS:???
 *		VNOP_OPEN:???
	if ((action & KAUTH_VNODE_ACCESS) && !vfs_authopaqueaccess(vp->v_mount))
	if ((action & KAUTH_VNODE_EXECUTE) && (vp->v_type == VREG)) {



/*
 * Returns:	KAUTH_RESULT_ALLOW
 *		KAUTH_RESULT_DENY
 *
 * Imputed:	*arg3, modified		Error code in the deny case
 *		EROFS			Read-only file system
 *		EACCES			Permission denied
 *		EPERM			Operation not permitted [no execute]
 *	vnode_getattr:ENOMEM		Not enough space [only if has filesec]
 *	vnode_getattr:???
 *	vnode_authorize_opaque:*arg2	???
 *	vnode_authorize_checkimmutable:???
 *	vnode_authorize_delete:???
 *	vnode_authorize_simple:???
 */


static int
vnode_authorize_callback(kauth_cred_t cred, void *idata, kauth_action_t action,
			 uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
	vfs_context_t	ctx;
	vnode_t		cvp = NULLVP;
	vnode_t		vp, dvp;
	int		result;

	ctx = (vfs_context_t)arg0;
	vp = (vnode_t)arg1;
	dvp = (vnode_t)arg2;

	/*
	 * if there are 2 vnodes passed in, we don't know at
	 * this point which rights to look at based on the 
	 * combined action being passed in... defer until later...
	 * otherwise check the kauth 'rights' cache hung
	 * off of the vnode we're interested in... if we've already
	 * been granted the right we're currently interested in,
	 * we can just return success... otherwise we'll go through
	 * the process of authorizing the requested right(s)... if that
	 * succeeds, we'll add the right(s) to the cache.
	 * VNOP_SETATTR and VNOP_SETXATTR will invalidate this cache
	 */
        if (dvp && vp)
	        goto defer;
	if (dvp)
	        cvp = dvp;
	else
	        cvp = vp;

	if (vnode_cache_is_authorized(cvp, ctx, action) == TRUE)
	        return KAUTH_RESULT_ALLOW;
defer:
        result = vnode_authorize_callback_int(cred, idata, action, arg0, arg1, arg2, arg3);

	if (result == KAUTH_RESULT_ALLOW && cvp != NULLVP)
	        vnode_cache_authorized_action(cvp, ctx, action);

	return result;
}


vnode_authorize_callback_int(__unused kauth_cred_t unused_cred, __unused void *idata, kauth_action_t action,
	boolean_t		parent_authorized_for_delete = FALSE;
	boolean_t		found_deny = FALSE;
	boolean_t		parent_ref= FALSE;
	/*
	 * Note that we authorize against the context, not the passed cred
	 * (the same thing anyway)
	 */
		/*
		 * check to see if we've already authorized the parent
		 * directory for deletion of its children... if so, we
		 * can skip a whole bunch of work... we will still have to
		 * authorize that this specific child can be removed
		 */
		if (vnode_cache_is_authorized(dvp, ctx, KAUTH_VNODE_DELETE) == TRUE)
		        parent_authorized_for_delete = TRUE;
	if ((rights & KAUTH_VNODE_EXECUTE) && (vp->v_type == VREG) && (vp->v_mount->mnt_flag & MNT_NOEXEC)) {
	if ((vp->v_mount->mnt_kern_flag & MNTK_AUTH_OPAQUE) && vnode_authorize_opaque(vp, &result, action, ctx))
	if (dvp && parent_authorized_for_delete == FALSE) {
	if (S_ISXATTR(va.va_mode) || vnode_isnamedstream(vp)) {

	/*
	 * Point 'vp' to the resource fork's parent for ACL checking
	 */
	if (vnode_isnamedstream(vp) &&
	    (vp->v_parent != NULL) &&
	    (vget_internal(vp->v_parent, 0, VNODE_NODEAD) == 0)) {
		parent_ref = TRUE;
		vcp->vp = vp = vp->v_parent;
		if (VATTR_IS_SUPPORTED(&va, va_acl) && (va.va_acl != NULL))
			kauth_acl_free(va.va_acl);
		VATTR_INIT(&va);
		VATTR_WANTED(&va, va_mode);
		VATTR_WANTED(&va, va_uid);
		VATTR_WANTED(&va, va_gid);
		VATTR_WANTED(&va, va_flags);
		VATTR_WANTED(&va, va_acl);
		if ((result = vnode_getattr(vp, &va, ctx)) != 0)
			goto out;
	}

	    parent_authorized_for_delete == FALSE &&
		    parent_authorized_for_delete == FALSE &&
		    (result = vnode_authorize_simple(vcp, rights, rights & KAUTH_VNODE_DELETE, &found_deny)) != 0)

		if (parent_ref)
			vnode_put(vp);
	if ((rights & KAUTH_VNODE_SEARCH) && found_deny == FALSE && vp->v_type == VDIR) {
	        /*
		 * if we were successfully granted the right to search this directory
		 * and there were NO ACL DENYs for search and the posix permissions also don't
		 * deny execute, we can synthesize a global right that allows anyone to 
		 * traverse this directory during a pathname lookup without having to
		 * match the credential associated with this cache of rights.
		 */
	        if (!VATTR_IS_SUPPORTED(&va, va_mode) ||
		    ((va.va_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) ==
		     (S_IXUSR | S_IXGRP | S_IXOTH))) {
		        vnode_cache_authorized_action(vp, ctx, KAUTH_VNODE_SEARCHBYANYONE);
		}
	}
	if ((rights & KAUTH_VNODE_DELETE) && parent_authorized_for_delete == FALSE) {
	        /*
		 * parent was successfully and newly authorized for deletions
		 * add it to the cache
		 */
	        vnode_cache_authorized_action(dvp, ctx, KAUTH_VNODE_DELETE);
	}
	if (parent_ref)
		vnode_put(vp);
	int		has_priv_suser, ismember, defaulted_owner, defaulted_group, defaulted_mode;
	if (vfs_authopaque(dvp->v_mount))
		has_priv_suser = 1;
		has_priv_suser = vfs_context_issuser(ctx);
		if (has_priv_suser) {
	if (!has_priv_suser) {
 * Check that the attribute information in vap can be legally written by the
 * context.
 * Call this when you're not sure about the vnode_attr; either its contents
 * have come from an unknown source, or when they are variable.
	int		error, has_priv_suser, ismember, chowner, chgroup, clear_suid, clear_sgid;
	if (vfs_authopaque(vp->v_mount))
	has_priv_suser = kauth_cred_issuser(cred);
		if (has_priv_suser || vauth_node_owner(&ova, cred)) {
		if (has_priv_suser) {
			if (!has_priv_suser) {
			if (!has_priv_suser) {
			if (has_priv_suser) {
	clear_suid = 0;
	clear_sgid = 0;
	if (VATTR_IS_ACTIVE(vap, va_uid)) {
		if (VATTR_IS_SUPPORTED(&ova, va_uid) && (vap->va_uid != ova.va_uid)) {
		if (!has_priv_suser && (kauth_cred_getuid(cred) != vap->va_uid)) {
		clear_suid = 1;
	}
	if (VATTR_IS_ACTIVE(vap, va_gid)) {
		if (VATTR_IS_SUPPORTED(&ova, va_gid) && (vap->va_gid != ova.va_gid)) {
		if (!has_priv_suser) {
		clear_sgid = 1;
	}
		if (!has_priv_suser) {
		clear_suid = 1;
		if (!has_priv_suser) {
	if (chowner || chgroup || clear_suid || clear_sgid) {
		if (has_priv_suser) {
void
vn_setunionwait(vnode_t vp)
{
	vnode_lock_spin(vp);
	vp->v_flag |= VISUNION;
	vnode_unlock(vp);
}


void
vn_checkunionwait(vnode_t vp)
{
	vnode_lock(vp);
	while ((vp->v_flag & VISUNION) == VISUNION)
		msleep((caddr_t)&vp->v_flag, &vp->v_lock, 0, 0, 0);
	vnode_unlock(vp);
}

void
vn_clearunionwait(vnode_t vp, int locked)
{
	if (!locked)
		vnode_lock(vp);
	if((vp->v_flag & VISUNION) == VISUNION) {
		vp->v_flag &= ~VISUNION;
		wakeup((caddr_t)&vp->v_flag);
	}
	if (!locked)
		vnode_unlock(vp);
}

/*
 * XXX - get "don't trigger mounts" flag for thread; used by autofs.
 */
extern int thread_notrigger(void);

int
thread_notrigger(void)
{
	struct uthread *uth = (struct uthread *)get_bsdthread_info(current_thread());
	return (uth->uu_notrigger);
}

/* 
 * Removes orphaned apple double files during a rmdir
 * Works by:
 * 1. vnode_suspend().
 * 2. Call VNOP_READDIR() till the end of directory is reached.  
 * 3. Check if the directory entries returned are regular files with name starting with "._".  If not, return ENOTEMPTY.  
 * 4. Continue (2) and (3) till end of directory is reached.
 * 5. If all the entries in the directory were files with "._" name, delete all the files.
 * 6. vnode_resume()
 * 7. If deletion of all files succeeded, call VNOP_RMDIR() again.
 */

errno_t rmdir_remove_orphaned_appleDouble(vnode_t vp , vfs_context_t ctx, int * restart_flag) 
{

#define UIO_BUFF_SIZE 2048
	uio_t auio = NULL;
	int eofflag, siz = UIO_BUFF_SIZE, nentries = 0;
	int open_flag = 0, full_erase_flag = 0;
	char uio_buf[ UIO_SIZEOF(1) ];
	char *rbuf = NULL, *cpos, *cend;
	struct nameidata nd_temp;
	struct dirent *dp;
	errno_t error;

	error = vnode_suspend(vp);

	/*
	 * restart_flag is set so that the calling rmdir sleeps and resets
	 */
	if (error == EBUSY)
		*restart_flag = 1;
	if (error != 0)
		goto outsc;

	/*
	 * set up UIO
	 */
	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
	if (rbuf)
		auio = uio_createwithbuffer(1, 0, UIO_SYSSPACE, UIO_READ,
				&uio_buf[0], sizeof(uio_buf));
	if (!rbuf || !auio) {
		error = ENOMEM;
		goto outsc;
	}

	uio_setoffset(auio,0);

	eofflag = 0;

	if ((error = VNOP_OPEN(vp, FREAD, ctx))) 
		goto outsc; 	
	else
		open_flag = 1;

	/*
	 * First pass checks if all files are appleDouble files.
	 */

	do {
		siz = UIO_BUFF_SIZE;
		uio_reset(auio, uio_offset(auio), UIO_SYSSPACE, UIO_READ);
		uio_addiov(auio, CAST_USER_ADDR_T(rbuf), UIO_BUFF_SIZE);

		if((error = VNOP_READDIR(vp, auio, 0, &eofflag, &nentries, ctx)))
			goto outsc;

		if (uio_resid(auio) != 0) 
			siz -= uio_resid(auio);

		/*
		 * Iterate through directory
		 */
		cpos = rbuf;
		cend = rbuf + siz;
		dp = (struct dirent*) cpos;

		if (cpos == cend)
			eofflag = 1;

		while ((cpos < cend)) {
			/*
			 * Check for . and .. as well as directories
			 */
			if (dp->d_ino != 0 && 
					!((dp->d_namlen == 1 && dp->d_name[0] == '.') ||
					    (dp->d_namlen == 2 && dp->d_name[0] == '.' && dp->d_name[1] == '.'))) {
				/*
				 * Check for irregular files and ._ files
				 * If there is a ._._ file abort the op
				 */
				if ( dp->d_namlen < 2 ||
						strncmp(dp->d_name,"._",2) ||
						(dp->d_namlen >= 4 && !strncmp(&(dp->d_name[2]), "._",2))) {
					error = ENOTEMPTY;
					goto outsc;
				}
			}
			cpos += dp->d_reclen;
			dp = (struct dirent*)cpos;
		}
		
		/*
		 * workaround for HFS/NFS setting eofflag before end of file 
		 */
		if (vp->v_tag == VT_HFS && nentries > 2)
			eofflag=0;

		if (vp->v_tag == VT_NFS) {
			if (eofflag && !full_erase_flag) {
				full_erase_flag = 1;
				eofflag = 0;
				uio_reset(auio, 0, UIO_SYSSPACE, UIO_READ);
			}
			else if (!eofflag && full_erase_flag)
				full_erase_flag = 0;
		}

	} while (!eofflag);
	/*
	 * If we've made it here all the files in the dir are AppleDouble 
	 * We can delete the files even though the node is suspended
	 * because we are the owner of the file.
	 */

	uio_reset(auio, 0, UIO_SYSSPACE, UIO_READ);
	eofflag = 0;
	full_erase_flag = 0;

	do {
		siz = UIO_BUFF_SIZE;
		uio_reset(auio, uio_offset(auio), UIO_SYSSPACE, UIO_READ);
		uio_addiov(auio, CAST_USER_ADDR_T(rbuf), UIO_BUFF_SIZE);

		error = VNOP_READDIR(vp, auio, 0, &eofflag, &nentries, ctx);

		if (error != 0) 
			goto outsc;

		if (uio_resid(auio) != 0) 
			siz -= uio_resid(auio);

		/*
		 * Iterate through directory
		 */
		cpos = rbuf;
		cend = rbuf + siz;
		dp = (struct dirent*) cpos;
		
		if (cpos == cend)
			eofflag = 1;
	
		while ((cpos < cend)) {
			/*
			 * Check for . and .. as well as directories
			 */
			if (dp->d_ino != 0 && 
					!((dp->d_namlen == 1 && dp->d_name[0] == '.') ||
					    (dp->d_namlen == 2 && dp->d_name[0] == '.' && dp->d_name[1] == '.'))
					  ) {
				NDINIT(&nd_temp, DELETE, USEDVP, UIO_SYSSPACE, CAST_USER_ADDR_T(dp->d_name), ctx);
				nd_temp.ni_dvp = vp;
				error = unlink1(ctx, &nd_temp, 0);
				if(error && error != ENOENT)
					goto outsc;
			}
			cpos += dp->d_reclen;
			dp = (struct dirent*)cpos;
		}
		
		/*
		 * workaround for HFS/NFS setting eofflag before end of file 
		 */
		if (vp->v_tag == VT_HFS && nentries > 2)
			eofflag=0;

		if (vp->v_tag == VT_NFS) {
			if (eofflag && !full_erase_flag) {
				full_erase_flag = 1;
				eofflag = 0;
				uio_reset(auio, 0, UIO_SYSSPACE, UIO_READ);
			}
			else if (!eofflag && full_erase_flag)
				full_erase_flag = 0;
		}

	} while (!eofflag);


	error = 0;

outsc:
	if (open_flag)
		VNOP_CLOSE(vp, FREAD, ctx);

	uio_free(auio);
	FREE(rbuf, M_TEMP);

	vnode_resume(vp);


	return(error);

}
