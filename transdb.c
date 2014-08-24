
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

/* ROZWIAZANIE:
	Z treści zadania - 3 warunki poprawnej bazy danych:
	1. Transakcja kończy się, gdy zostanie zatwierdzona lub porzucona przez 
	   użytkownika. Zmiany dokonane w zatwierdzonej transakcji permanentnie stają 
		się częścią bazy danych, a zmiany dokonane w porzuconej transkacji giną 
		bezpowrotnie.
	2. Transakcje widzą wyniki wcześniejszych zatwierdzonych transakcji, ale nie 
	   widzą zmian z niezatwierdzonych transakcji.
	3. “Stan świata” widziany przez transakcję nie zmienia się w trakcie jej trwania.

	Dostęp do bazy danych na zasadzie "Czytelnicy i Pisarze".
	Proces, który otwiera /dev/db wiesza się na semaforze typu "Czytelnik".
	Wszystkie operacje write są zapisywane na liście, będą wykonane, dopiero gdy
	transakcja będzie zatwierdzona(wymaga tego warunek 2.).
	Transakcje mogą być zatwierdzone dopiero jak wszystkie otwarte transakcje
	zostaną porzucone, lub wysałane do zatwierdzenia(wymaga tego warunek 3.)
	Z warunku 3. nie będzie nigdy sytuacji, że przy write wiadomo już, że
	transakcja nie ma szans się udać. 
	
	Tworzenie transakcji:
		- Czekam na semafor "Czytelnik"
		- Zwiększam licznik transakcji o jeden(db_counter)
		- Nadaje transakcji numer(db_counter)
		
	Zatwierdzanie transakcji:
		1. Zwalniam semafor "Czytelnik"
		2. Czekam na semafor "Pisarz"
		3. Sprawdzam, czy numer mojej transakcji jest większy od db_timestamp
		4. Jeśli jest, to ustalam db_timestamp na db_counter i realizuje zaksięgowane
		   operacje write dotyczące tej transakcji
		- Proszę zwrócić uwagę, że w kroku 4. wszystkie otwarte transakcje zostały 
		  utworzone przed zapisaniem, tej transakcji, więc będą musiały być porzucone.

TODO:
	Dane w bazie danych podzieliłem na 32 bajtowe bloki, każdy blok ma 4 bajty na
	metadane: znaczniki czasu. 
	*/

/* HIERARCHIA SEMAFOROW
	1. Czytelnicy/pisarze.
	2. Semafor transakcyjny.
	3. Semafor globalny.
	Przykład:
	Nie można blokować 2. mając zablokowany 3.
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
#include "transdb.h"

MODULE_AUTHOR("Krzysztof Leszczynski kl319367@students.mimuw.edu.pl");
MODULE_DESCRIPTION("Database");
MODULE_LICENSE("GPL");

#define MAX_SIZE 1024024
#define ACCEPT 1
#define ROLLBACK 2
#define db_minor 123

dev_t db_dev;
int db_major;
struct class *db_class;
struct device *db_device;
uint8_t db[MAX_SIZE];
struct rw_semaphore db_rwsema;
struct semaphore db_glsema;
uint32_t db_timestamp, db_counter;

struct needle {
	loff_t pos;
	uint8_t data;
	struct list_head list;
};

struct transaction {
	uint32_t id;
	struct list_head list; //list of needles
	struct semaphore sema;
};

void is_active(struct transaction *trans) {
	char bool = 0;
	down(&trans->sema);
	if(trans->id == 0) {
		bool = 1;
	}
	up(&trans->sema);
	if(bool) {
		down_read(&db_rwsema);
		down(&trans->sema);
		down(&db_glsema);
		db_counter++;
		trans->id = db_counter;
		up(&db_glsema);
		up(&trans->sema);
	}
}

int transdb_open(struct inode *inode, struct file *file) {
	struct transaction *trans = kmalloc(sizeof(struct transaction), GFP_KERNEL);
	if(trans == 0) goto error;
	INIT_LIST_HEAD(&trans->list);
	sema_init(&trans->sema, 1);

	down_read(&db_rwsema);
	down(&db_glsema);
	db_counter++;
	trans->id = db_counter;
	up(&db_glsema);

	file->private_data = trans;
	printk(KERN_WARNING "Transdb open %d\n", trans->id);
	
	return 0;
error:
	return -ENOMEM;
}

ssize_t transdb_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	struct transaction *trans = (struct transaction*)filp->private_data;
	is_active(trans);

	if(copy_to_user(buff, db + *offp, count)) return -EFAULT;
	*offp += count;
	return count;
}

ssize_t transdb_write(struct file *filp, const char __user *buff, size_t count,
		                loff_t *offp) {
	struct transaction *trans = (struct transaction *)(filp->private_data);
	struct needle *op;
	int size = count;
	is_active(trans);
	down(&trans->sema);
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
	up(&trans->sema);
	return size - count;
}

void delete_list_with_lock(struct transaction *trans) {		
	struct needle *pos;
	struct needle *temp;
	
	list_for_each_entry_safe(pos, temp, &trans->list, list) {			
		list_del(&pos->list);
		kfree(pos);
	}
	trans->id = 0;
	up(&trans->sema);
}
void delete_list(struct transaction *trans) {
	down(&trans->sema);
	delete_list_with_lock(trans);
}

int accept_trans(struct transaction *trans) {
	struct needle *pos;
	struct needle *temp;

	up_read(&db_rwsema);
	down_write(&db_rwsema); 
	down(&trans->sema);
	printk(KERN_WARNING "Accept trans: %d\n", trans->id);
	down(&db_glsema);

	if(trans->id <= db_timestamp) {
		up(&db_glsema);
		up_write(&db_rwsema);
		delete_list_with_lock(trans);
		return -EDEADLK;
	}
	db_timestamp = db_counter;
	up(&db_glsema);
	trans->id = 0;

	list_for_each_entry_safe(pos, temp, &trans->list, list) {
		db[pos->pos] = pos->data;
		list_del(&pos->list);
		kfree(pos);
	}

	up(&trans->sema);
	up_write(&db_rwsema);
	return 0;
}
void rollback_trans(struct transaction *trans) {
	up_read(&db_rwsema);
	delete_list(trans);
}

int transdb_release(struct inode *inode, struct file *filp) {
	struct transaction *trans = (struct transaction*)filp->private_data;
	printk(KERN_WARNING "Transdb release\n");
	if(trans->id != 0) rollback_trans(trans);
	kfree(trans);
	return 0;
}

long transdb_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	struct transaction *trans = (struct transaction*)filp->private_data;
	long ret = 0;
	switch(cmd) {
		case DB_COMMIT:
			ret = accept_trans(trans);
			break;
		case DB_ROLLBACK:
			rollback_trans(trans);
	}
	return ret;
}

const static struct file_operations db_fops = {
	owner: THIS_MODULE,
	open: transdb_open,
	read: transdb_read,
	//pread: transdb_pread,
	write: transdb_write,
	//pwrite: transdb_pwrite,
	//lseek: transdb_lseek,
	release: transdb_release,
	unlocked_ioctl: transdb_ioctl,
	compat_ioctl: transdb_ioctl
};

int transdb_init_module(void) {
 	int res;
	if((res = register_chrdev(0, "db", &db_fops)) < 0)
		goto error;
	db_major = res;
	db_dev = MKDEV(db_major, db_minor); 
	db_class = class_create(THIS_MODULE, "transdb");
	if(IS_ERR(db_class)) {
		res = PTR_ERR((void*)db_class);
		goto error2;
	}
	db_device = device_create(db_class, 0, db_dev, 0, "db");
	if(IS_ERR(db_device)) {
		res = PTR_ERR(db_device);
		goto error2;
	}

	printk(KERN_WARNING "Module init %d\n", db_major);
	init_rwsem(&db_rwsema);
	sema_init(&db_glsema, 1);
	memset(db, MAX_SIZE, 0);
	
	return 0;
error2:
	unregister_chrdev(db_major, "db");
error:
	return res;
}

void transdb_cleanup_module(void) {
	unregister_chrdev(db_major, "db");
	device_destroy(db_class, db_dev);
	class_destroy(db_class);
}

module_init(transdb_init_module);
module_exit(transdb_cleanup_module);
