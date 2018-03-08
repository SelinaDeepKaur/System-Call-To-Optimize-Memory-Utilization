#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include "utils.h"
#define  EXTRA_CREDIT
#define FLAG_N  0x01 // decimal 1
#define FLAG_D  0x02 // decimal 2
#define FLAG_P  0x04 // decimal 4
int DFLAG = 0;
#define bugPs(s) if(DFLAG) printk("%s\n",s);
#define bugPsd(s,d) if(DFLAG) printk("%s, %d\n",s,d);

asmlinkage extern long (*sysptr)(void *arg);

bool checkHardlink(struct file *fp1,struct file *fp2) {
    struct super_block *sb1, *sb2;
    struct inode *inode1, *inode2;
    unsigned long f1inode_no, f2inode_no;

    inode1 = fp1->f_path.dentry->d_inode;
    inode2 = fp2->f_path.dentry->d_inode;
    sb1 = inode1->i_sb;
    sb2 = inode2->i_sb;
    f1inode_no=inode1->i_ino;
    f2inode_no=inode2->i_ino;
    if ((f1inode_no == f2inode_no) && (sb1->s_uuid == sb2->s_uuid)) {
        return true;
    }
    else
        return false;
}

/**
 *
 * @param data - data buffer containing data to be written
 * @param fpout - pointer to the file to which the data has to be written
 * @param sz - size of data to be written
 * @param offset - offset of the file
 * @return - error code (negative value)/ no of bytes written
 *
 * Function to write data to a file using vfs_write
 */
int write_data(unsigned char *data, struct file *fpout,
               size_t sz, loff_t *offset) {
    int ret;
    mm_segment_t oldfs;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    ret = vfs_write(fpout,data,sz,offset);

    set_fs(oldfs);
    return ret;
}

/**
 *
 * @param f - file to be read
 * @param offset - offset for the file
 * @param data - data buffer
 * @param size - pagesize
 * @return - error code (negative value)/ no of bytes written
 *
 * Function to read data from a file using vfs_read
 */
int file_read(struct file *f, loff_t *offset,
              unsigned char *data, unsigned int size) {
    int error_code=0;
    mm_segment_t oldfs;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    error_code=vfs_read(f, data, size, offset);

    set_fs(oldfs);
    return error_code;
}

/**
 *
 * @param file - file to be unlinked
 * @return - error code/ 0
 *
 * Function that unlinks the file supplied
 */
int unlink_files(struct file *file) {
    int error_code=0;
    mutex_lock_nested(&(file->f_path.dentry->d_parent->d_inode->i_mutex),
                      I_MUTEX_PARENT);
    error_code=vfs_unlink(file->f_path.dentry->d_parent->d_inode,
                          file->f_path.dentry,NULL);
    mutex_unlock(&(file->f_path.dentry->d_parent->d_inode->i_mutex));
    file->f_path.dentry->d_inode=NULL;
    return error_code;
}

/**
 *
 * @param f2name - file2, which is to be unlinked
 * @param file1 - file1, to which file2 will be linked
 * @return - error code/ 0
 *
 * Functions that links file 2 to file 1
 */
int link_files(char *f2name,struct file *file1) {
    int error_code=0;
    struct dentry *new_dentry;
    struct path path_new;
    mm_segment_t oldfs;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    new_dentry = user_path_create(AT_FDCWD, f2name, &path_new , 0);
    error_code=vfs_link(file1->f_path.dentry, path_new.dentry->d_inode,
                        new_dentry,NULL);
    done_path_create(&path_new, new_dentry);
    set_fs(oldfs);
    path_put(&(file1->f_path));
    return error_code;
}

/**
 *
 * @param fp1 - file1
 * @param fp2 - file2
 * @param f1size - size of file1
 * @param f2size - size of file2
 * @param f1name - name of file1
 * @param f2name - name of file2 (Sending the absolute path to handle symlinks)
 * @param flagN - n flag (1 if user supplies -n otherwise 0)
 * @return - error code/ size of one of the files
 * (if the two files have same data)
 *
 * This function is invoked when the user supplies no flag.
 * The two given files are compared byte and byte. If the data
 * in file 1 matches the data in file 2 completely,
 * file 2 is unlinked and then hard-linked to file 1.
 * The case of symlinks is handled by passing absolute path
 * of file 2 to file2 name pointer. File2 is linked to file 1
 * using the absolute path
 */
int completeFileRead(struct file *fp1,struct file *fp2,off_t f1size,
                      off_t f2size, char *f1name, char *f2name, int flagN) {
    int error_code=0, ret=0, flag=0, ret1, ret2, maxsf;
    unsigned char *data1, *data2;
    unsigned long long offst1=0, offst2=0;
    loff_t *of1= &offst1, *of2= &offst2;

    if(f1size!=f2size)
    {
        bugPs("The size of the two files is different");
        error_code= -1;
        goto EXIT;
    }

    data1 =(unsigned char *) kmalloc(PAGE_SIZE,GFP_KERNEL);
    if(data1==NULL)
    {
        error_code=-ENOMEM;
        goto EXIT;
    }

    data2 =(unsigned char *) kmalloc(PAGE_SIZE,GFP_KERNEL);
    if(data2==NULL)
    {
        error_code=-ENOMEM;
        goto EXIT1;
    }

    while(1) {
        memset(data1, 0, PAGE_SIZE);
        memset(data2, 0, PAGE_SIZE);

        ret1 = file_read(fp1, of1, data1, PAGE_SIZE);
        ret2 = file_read(fp2, of2, data2, PAGE_SIZE);

        if(ret1<=0 || ret2<=0)
            break;

        if(ret1>ret2)
            maxsf=ret1;
        else
            maxsf=ret2;

        flag = memcmp((void*)data1,(void*)data2,maxsf);

        if (flag != 0) {
            break;
        }
    }

    if(flag!=0 || ret1<0 || ret2<0) {
        bugPs("Cannot deduplicate");
        error_code = (flag||ret1||ret2);
        goto EXIT2;
    }
    else
    {
        bugPs("Deduplication possible");
        if(flagN)
        {
            error_code=f1size;
            goto EXIT2;
        }

        ret=unlink_files(fp2) || link_files(f2name,fp1);

        if(ret<0) {
            error_code = ret;
            goto EXIT2;
        }
        else {
            error_code = f1size;
        }
    }

    EXIT2:
    kfree(data2);
    EXIT1:
    kfree(data1);
    EXIT:
    return error_code;
}

/***
 *
 * @param fp1 - file1
 * @param fp2 - file2
 * @param fpo - pointer to outfile
 * @param outname
 * @param N_flag
 * @param hardLink
 * @return
 */
int partialData(struct file *fp1,struct file *fp2, struct file *fpo,
                char *outname, int N_flag, int hardLink) {
    int error_code=0,ret1,ret2, minsf,flag=0,outfileExists=0,bytes=0;
    unsigned char *data1,*data2;
    size_t fsize=0,i;
    unsigned long long offst1=0, offst2=0, offst3=0;
    loff_t *of1= &offst1, *of2= &offst2, *of3=&offst3;
    char *tmp;
    struct file *d_t=NULL, *check=NULL;
    struct dentry *dentry_out,*dentry_tmp,*dentry_out_par,*dentry_tmp_par;
    umode_t mode=0;

    data1 =(unsigned char *) kmalloc(PAGE_SIZE,GFP_KERNEL);
    if(data1==NULL)
    {
        error_code=-ENOMEM;
        goto EXIT1;
    }

    data2 =(unsigned char *) kmalloc(PAGE_SIZE,GFP_KERNEL);
    if(data2==NULL)
    {
        error_code=-ENOMEM;
        goto EXIT2;
    }

    if(!N_flag) {
        check = filp_open(outname, O_WRONLY|AT_SYMLINK_FOLLOW, 0);
        if (check == NULL || IS_ERR(check)) {
            error_code = PTR_ERR(check);
            if (error_code == -ENOENT) {
                mode = fp1->f_path.dentry->d_inode->i_mode & (0777);
                mode = mode & fp2->f_path.dentry->d_inode->i_mode;
            } else {
                goto EXIT3;
            }
        } else {
            outfileExists=1;
            mode = check->f_path.dentry->d_inode->i_mode & (0777);
        }

        tmp=outname;
        strcat(tmp,"_tmp");
        strcat(tmp,__TIMESTAMP__);

        d_t = filp_open(tmp,O_WRONLY|O_CREAT|O_TRUNC, mode);
        if (d_t==NULL || IS_ERR(d_t)) {
            bugPsd("Error in opening temp file with given permission",
                   (int) PTR_ERR(d_t));
            error_code = PTR_ERR(d_t);

            if(outfileExists)
                goto EXIT4;
            else
                goto EXIT3;
        }
    }
    if(hardLink)
    {
        while(1) {
            memset(data1, 0, PAGE_SIZE);
            bytes = file_read(fp1, of1, data1, PAGE_SIZE);

            if(bytes<0) {
                bugPs("Error in reading the file");
                error_code = bytes;
                goto EXIT3;
            }

            if(bytes==0) {
                break;
            }

            error_code=write_data(data1,d_t,bytes,of3);
            if(error_code<0) {
                bugPs("Error error in writing data to the file");
                goto EXIT3;
            }
        }
    }
    else {
        while (1) {
            memset(data1, 0, PAGE_SIZE);
            memset(data2, 0, PAGE_SIZE);
            ret1 = file_read(fp1, of1, data1, PAGE_SIZE);
            ret2 = file_read(fp2, of2, data2, PAGE_SIZE);

            if (ret1 <= 0 || ret2 <= 0) {
                if (ret1 < 0) {
                    error_code = ret1;
                } else if (ret2 < 0) {
                    error_code = ret2;
                } else {
                    error_code = fsize;
                    if(!N_flag)
                        goto abc;
                    else
                        goto EXIT3;
                }
                if (outfileExists)
                    goto EXIT4;
                else
                    goto EXIT3;
            }

            if (ret1 <= ret2) {
                minsf = ret1;
            } else {
                minsf = ret2;
            }

            i = 0;
            while (i < minsf) {
                if (data1[i] != data2[i]) {
                    flag = 1;
                    break;
                }
                i++;
            }

            if (!N_flag) {
                error_code = write_data(data1, d_t, i, of3);
                if (error_code < 0) {
                    bugPs("Error encountered while writing data to the file");
                    filp_close(d_t, NULL);
                    if (outfileExists)
                        goto EXIT4;
                    else
                        goto EXIT3;
                }
            }
            fsize += i;
            if(flag) {
                break;
            }
        }
    }
    if(N_flag) {
        error_code=fsize;
        goto EXIT3;
    }
    abc:
    dentry_out=fpo->f_path.dentry;
    dentry_tmp=d_t->f_path.dentry;
    dentry_out_par=dget_parent(dentry_out);
    dentry_tmp_par=dget_parent(dentry_tmp);
    lock_rename(dentry_out_par,dentry_tmp_par);
    error_code=vfs_rename(dentry_tmp_par->d_inode,dentry_tmp,
                          dentry_out_par->d_inode,dentry_out,NULL,0);
    unlock_rename(dentry_out_par,dentry_tmp_par);

    if(error_code==0) {
        if(hardLink)
            error_code = fp1->f_path.dentry->d_inode->i_size;
        else
            error_code = fsize;
        filp_close(d_t,NULL);
        if(outfileExists)
            goto EXIT4;
        else
            goto EXIT3;
    }
    else
    {
        unlink_files(d_t);
        if(!outfileExists)
            unlink_files(check);
        goto EXIT3;
    }

    EXIT4:
    filp_close(check,NULL);
    EXIT3:
    kfree(data2);
    EXIT2:
    kfree(data1);
    EXIT1:
    return error_code;
}

asmlinkage long xdedup(void *arg) {
    int error_code=0;
    struct file *fpo, *fp1, *fp2;
    struct inode *inode1, *inode2;
    off_t f1size, f2size;
    char *ptr1, *ptr2;

    struct myargs *argsList = (struct myargs *)
            kmalloc(sizeof(struct myargs), GFP_KERNEL);
    if(argsList==NULL) {
        error_code = -ENOMEM;
        goto EXIT;
    }

    error_code=copy_from_user(argsList,arg, sizeof(struct myargs));
    if(error_code!=0)
        goto EXIT1;

    if(argsList->flag & FLAG_D) {
        DFLAG=1;
    }

    bugPs("\n--------------------Inside System Call---------------------");

    if (argsList->f1 == NULL || argsList->f2 == NULL) {
        error_code = -EINVAL;
        goto EXIT1;
    }

    fp1 = filp_open(argsList->f1, O_RDONLY|AT_SYMLINK_FOLLOW, 0);
    if (fp1 == NULL || IS_ERR(fp1)) {
        bugPsd("Error in reading the file", (int) PTR_ERR(fp1));
        error_code = PTR_ERR(fp1);
        goto EXIT1;
    }
    fp2 = filp_open(argsList->f2, O_RDONLY|AT_SYMLINK_FOLLOW, 0);
    if (fp2 == NULL || IS_ERR(fp2)) {
        bugPsd("Error in reading the file", (int) PTR_ERR(fp2));
        error_code = PTR_ERR(fp2);
        goto EXIT2;
    }

    ptr1=(char *)kmalloc(PAGE_SIZE,GFP_KERNEL);
    if(ptr1==NULL)
    {
        error_code=-ENOMEM;
        goto EXIT3;
    }
    ptr2=d_path(&fp2->f_path,ptr1,PAGE_SIZE);

    inode1 = fp1->f_path.dentry->d_inode;
    inode2 = fp2->f_path.dentry->d_inode;
    f1size = inode1->i_size;
    f2size = inode2->i_size;

    error_code = S_ISDIR(inode1->i_mode);
    if(error_code!=0) {
        bugPs("File1 is a directory");
        error_code=-EISDIR;
        goto EXIT3;
    }

    error_code = S_ISDIR(inode2->i_mode);
    if(error_code!=0) {
        bugPs("File2 is a directory");
        error_code=-EISDIR;
        goto EXIT3;
    }

    if(S_ISREG(inode1->i_mode)==0) {
        error_code=-EINVAL;
        bugPs("File1 is not regular");
        goto EXIT3;
    }

    if(S_ISREG(inode2->i_mode)==0) {
        error_code=-EINVAL;
        bugPs("File2 is not regular");
        goto EXIT3;
    }

    if(!uid_eq(inode1->i_uid,inode2->i_uid) &&
            !(argsList->flag & FLAG_P) && !(argsList->flag & FLAG_N))
    {
        bugPs("Owners of the files are different");
        error_code=-1;
        goto EXIT3;
    }

    if (checkHardlink(fp1,fp2)) {
        bugPs("The two files are hard-linked");
        if((argsList->flag & FLAG_P) && !(argsList->flag & FLAG_N))
        {
            fpo = filp_open(argsList->outf, O_WRONLY | O_CREAT|AT_SYMLINK_FOLLOW, 0644);
            if (fpo == NULL || IS_ERR(fpo)) {
                bugPsd("Error in opening/creating the output file",
                       (int) PTR_ERR(fpo));
                error_code = PTR_ERR(fpo);
                goto EXIT3;
            }
            if(checkHardlink(fp1,fpo) || checkHardlink(fp2,fpo)) {
                error_code=-EINVAL;
                goto EXIT4;
            }

            error_code=partialData(fp1,NULL,fpo,argsList->outf,0,1);
            if(error_code==0) {
                error_code = f1size;
            }
            goto EXIT4;
        }
        if(argsList->flag & FLAG_N)
            error_code = f1size;
        else
            error_code=-EINVAL;
        goto EXIT3;
    }

    if (argsList->flag & FLAG_P) {

        if(argsList->flag & FLAG_N)
        {
            error_code = partialData(fp1,fp2,NULL,NULL,1,0);
            goto EXIT3;
        }
        else {
            fpo = filp_open(argsList->outf, O_WRONLY | O_CREAT | AT_SYMLINK_FOLLOW, 0644);
            if (fpo == NULL || IS_ERR(fpo)) {
                bugPsd("Error in opening/creating the output file",
                       (int) PTR_ERR(fpo));
                error_code = PTR_ERR(fpo);
                goto EXIT3;
            }
            if(checkHardlink(fp1,fpo) || checkHardlink(fp2,fpo)) {
                bugPs("The output file is hardlinked to one of the files");
                error_code=-EINVAL;
                goto EXIT4;
            }
            error_code = partialData(fp1, fp2, fpo, argsList->outf,0,0);
            goto EXIT4;
        }
    }

    if (!(argsList->flag & FLAG_P) && (f1size != f2size)) {
        bugPs("The size of the two files is different");
        error_code = -1;
        goto EXIT3;
    }

    error_code =  completeFileRead(fp1, fp2, f1size, f2size, argsList->f1,
                                   ptr2,argsList->flag & FLAG_N);
    goto EXIT3;

    EXIT4:
    filp_close(fpo,NULL);
    EXIT3:
    filp_close(fp2,NULL);
    EXIT2:
    filp_close(fp1,NULL);
    EXIT1:
    kfree(argsList);
    EXIT:
    return error_code;
}

static int __init init_sys_xdedup(void) {
    bugPs("installed new sys_xdedup module");
    if (sysptr == NULL)
        sysptr = xdedup;
    return 0;
}
static void  __exit exit_sys_xdedup(void) {
    if (sysptr != NULL)
        sysptr = NULL;
    bugPs("removed sys_xdedup module");
}
module_init(init_sys_xdedup);
module_exit(exit_sys_xdedup);
MODULE_LICENSE("GPL");
