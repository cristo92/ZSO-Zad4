
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

int transdb_major;
char db[MAX_SIZE];
rwlock_t db_rwlock;
char db_dirty;

struct needle {
	off_t pos;
	uint8_t data;
};

struct transaction {
	struct list_head list; //list of needles
};

char data[] = "siema\n";

ssize_t transdb_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	copy_to_user(data, buff, min(6, count));
	return min(count, 6);
}

int transdb_open(struct inode *inode, struct file *file) {
	struct transaction *trans = kmalloc(sizeof(struct transaction), GFP_KERNEL);
	if(trans == 0) goto error;
	INIT_LIST_HEAD(&trans->list);
	file->private_data = trans;
	printk(KERN_WARNING "Transdb open\n");
	
	return 0;
error:
	return -ENOMEM;
}

const static struct file_operations transdb_fops = {
	owner: THIS_MODULE,
	open: transdb_open,
	read: transdb_read/*,
	pread: transdb_pread,
	write: transdb_write,
	pwrite: transdb_pwrite,
	lseek: transdb_lseek,
	release: transdb_release,
	unlocked_ioctl: transdb_ioctl,
compat_ioctl: transdb_ioctl*/
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
