
/* Krzysztof Leszczyński
 * 319367
 */

/*
TODO:
	co to znaczy nieskonczona baza danych?
	*/

/* TESTOWANIE:
	- wielowatkowosc
	*/

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/rwlock_types.h>
#include <linux/list.h>
#include <asm/uaccess.h>

MODULE_AUTHOR("Krzysztof Leszczynski kl319367@students.mimuw.edu.pl");
MODULE_DESCRIPTION("Database");
MODULE_LICENSE("GPL");

#define MAX_SIZE 1024
#define ACCEPT 1
#define ROLLBACK 2

int transdb_major;
uint8_t db[MAX_SIZE];
rwlock_t db_rwlock;
char db_dirty;

struct needle {
	off_t pos;
	uint8_t data;
	struct list_head list;
};

struct transaction {
	struct list_head list; //list of needles
	struct semaphore sema;
};


int transdb_open(struct inode *inode, struct file *file) {
	struct transaction *trans = kmalloc(sizeof(struct transaction), GFP_KERNEL);
	if(trans == 0) goto error;
	INIT_LIST_HEAD(&trans->list);
	sema_init(&trans->sema, 1);
	file->private_data = trans;
	printk(KERN_WARNING "Transdb open\n");
	read_lock(&db_rwlock);
	
	return 0;
error:
	return -ENOMEM;
}

ssize_t transdb_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	printk(KERN_WARNING "Transdb read\n");
	if(copy_to_user(buff, db + *offp, count)) return -EFAULT;
	*offp += count;
	return count;
}

ssize_t transdb_write(struct file *filp, const char __user *buff, size_t count,
		                loff_t *offp) {
	struct transaction *trans = (struct transaction *)(filp->private_data);
	struct needle *op;
	int size = count;
	printk(KERN_WARNING "Transdb write\n");
	up(&trans->sema);
	while(count) {
		op = kmalloc(sizeof(struct needle), GFP_KERNEL); 
		if(op == 0) goto out;
		op->pos = *offp;
		if(copy_from_user(&op->data, buff, 1)) {
			kfree(op);
			goto out;
		}
		list_add(&op->list, &trans->list);
		count--;
		(*offp)++;
		buff++;
	}
out:
	down(&trans->sema);
	return size - count;
}

void finish_trans(struct transaction *trans, char flag) {
	read_unlock(&db_rwlock);
	down(&trans->sema);
	if(!list_empty(&trans->list)) {
		struct needle *pos;
		struct needle *temp;
		if(flag & ACCEPT) write_lock(&db_rwlock);
		list_for_each_entry_safe(pos, temp, &trans->list, list) {
			if(flag & ACCEPT) db[pos->pos] = pos->data;
			list_del(&pos->list);
			kfree(pos);
		}
		if(flag & ACCEPT) write_unlock(&db_rwlock);
	}
	up(&trans->sema);
}

void accept_trans(struct transaction *trans) {
	finish_trans(trans, ACCEPT);
}
void rollback_trans(struct transaction *trans) {
	finish_trans(trans, ROLLBACK);
}

int transdb_release(struct inode *inode, struct file *filp) {
	struct transaction *trans = (struct transaction*)filp->private_data;
	printk(KERN_WARNING "Transdb release\n");
	accept_trans(trans);
	return 0;
}

const static struct file_operations transdb_fops = {
	owner: THIS_MODULE,
	open: transdb_open,
	read: transdb_read,
	//pread: transdb_pread,
	write: transdb_write,
	//pwrite: transdb_pwrite,
	//lseek: transdb_lseek,
	release: transdb_release,
	//unlocked_ioctl: transdb_ioctl,
	//compat_ioctl: transdb_ioctl
};

int transdb_init_module(void) {
	if((transdb_major = register_chrdev(0, "transdb", &transdb_fops)) < 0)
		goto error;
	printk(KERN_WARNING "Module init %d\n", transdb_major);
	rwlock_init(&db_rwlock);
	db_dirty = 0;
	memset(db, MAX_SIZE, 0);
	
	return 0;
error:
	return transdb_major;
}

void transdb_cleanup_module(void) {
	unregister_chrdev(transdb_major, "transdb");
}

module_init(transdb_init_module);
module_exit(transdb_cleanup_module);
