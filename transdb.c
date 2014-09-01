
/* Krzysztof Leszczyński
 * 319367
 */

/*
TODO:
	co to znaczy nieskonczona baza danych?
	Test4 na llseek (nie umiem wywolac w userspace llseek)
	Test7 z watkami pthread
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

	Nieskończona baza danych:
		Bazę danych podzieliłem na 32-bajtowe bloki (struct db_block), które
		przechowuję w strukturze rbtree (z bibliotek kernela).
		Jak chcę się odwołać do jakiś danych w bazie danych, to najpierw lokalizuję
		blok, który przechowuje te dane, później szukam bloku w drzewie, a następnie
		jeśli bloku nie znalazłem to dodaję go do drzewa.
		Poza tym struktura db_block przechowuje 4-bajtowy znacznik czasu - używany do
		pozwalania na równoległe nieskonfliktowane zapisy.

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
#include <linux/rbtree.h>
#include <asm/uaccess.h>
#include "transdb.h"

MODULE_AUTHOR("Krzysztof Leszczynski kl319367@students.mimuw.edu.pl");
MODULE_DESCRIPTION("Database");
MODULE_LICENSE("GPL");

#define DB_SIZE 1024024
#define db_minor 123

dev_t db_dev;
int db_major;
struct class *db_class;
struct device *db_device;
uint8_t db[DB_SIZE];
struct rw_semaphore db_rwsema;
struct semaphore db_glsema;
uint32_t db_timestamp, db_counter;
uint8_t ZERO_DATA[DB_BLOCK_SIZE];
struct rb_root db_root;

struct db_block {
	/* It holds data in 
		(id << DB_BLOCK_SHIFT, id << DB_BLOCK_SHIFT + DB_BLOCK_SIZE - 1) */
	struct rb_node node;
	loff_t id;
	uint32_t ts; //timestamp
	uint8_t data[DB_BLOCK_SIZE];
};

struct needle {
	/* It write data in 
		(id << DB_BLOCK_SHIFT + beg, id << DB_BLOCK_SHIFT + end) */
	loff_t id;
	uint8_t beg, end;
	uint8_t data[DB_BLOCK_SIZE];
	struct list_head list;
};

struct transaction {
	uint32_t id;
	struct list_head list; //list of needles
	struct semaphore sema;
};

// Red-Black Tree functions
struct db_block *rb_search(struct rb_root *root, loff_t x)
{
  	struct rb_node *node = root->rb_node;

  	while (node) {
  		struct db_block *data = container_of(node, struct db_block, node);

		if(x < data->id)
			node = node->rb_left;
		else if(x > data->id)
			node = node->rb_right;
		else 
			return data;
	}
	return NULL;
}

int rb_insert(struct rb_root *root, struct db_block *data) 
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	while(*new) {
		struct db_block *this = container_of(*new, struct db_block, node);
		parent = *new;
		if(data->id < this->id) 
			new = &((*new)->rb_left);
		else if(data->id > this->id)
			new = &((*new)->rb_right);
		else
			return -1;
	}

	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);

	return 0;
}

///////////////////////////

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

	struct db_block *blk;
	size_t size = count;
	loff_t first = (*offp) >> DB_BLOCK_SHIFT;
	loff_t last = (*offp + count - 1) >> DB_BLOCK_SHIFT;
	loff_t i;
	uint8_t off_b;
	uint8_t off_e;

	for(i = first; i <= last; i++) {
		if(i == first) off_b = (*offp) & (DB_BLOCK_SIZE - 1);
		else off_b = 0;
		if(i == last) off_e = (*offp + count - 1) & (DB_BLOCK_SIZE - 1); 
		else off_e = DB_BLOCK_SIZE - 1;

		blk = rb_search(&db_root, i);
		if(blk == NULL) {
			if(copy_to_user(buff, ZERO_DATA, off_e - off_b + 1)) 
				goto out;
		}
		else {
			if(copy_to_user(buff, blk->data + off_b, off_e - off_b + 1))
				goto out;
		}

		buff += off_e - off_b + 1;
		count -= off_e - off_b + 1;
		*offp += off_e - off_b + 1;
	}

out:
	return size - count;
}
ssize_t transdb_pread(struct file *filp, char __user *buff, size_t count, loff_t offp) {
	int ret;
	ret = transdb_read(filp, buff, count, &offp);
	if(ret >= 0) offp -= ret;
	return ret;
}

ssize_t transdb_write(struct file *filp, const char __user *buff, size_t count,
		                loff_t *offp) {
	struct transaction *trans = (struct transaction *)(filp->private_data);
	struct needle *op;
	int size = count;
	is_active(trans);
	down(&trans->sema);

	loff_t first = (*offp) >> DB_BLOCK_SHIFT;
	loff_t last  = (*offp + count - 1) >> DB_BLOCK_SHIFT;
	loff_t i;
	uint8_t off_b = (*offp) & (DB_BLOCK_SIZE - 1);
	uint8_t off_e = (*offp + count - 1) & (DB_BLOCK_SIZE - 1);
	
	for(i = first; i <= last; i++) {
		op = kmalloc(sizeof(struct needle), GFP_KERNEL);
		if(op == 0) goto out;
		op->id = i;

		if(i == first) op->beg = off_b;
		else op->beg = 0;
		if(i == last) op->end = off_e;
		else op->end = DB_BLOCK_SIZE - 1;

		if(copy_from_user(op->data + op->beg, 
								buff, 
								op->end - op->beg + 1)) {
			kfree(op);
			goto out;
		}

		list_add(&op->list, &trans->list);
		count -= (op->end - op->beg + 1);
		buff += (op->end - op->beg + 1);
		*offp += (op->end - op->beg + 1);
	}

out:
	up(&trans->sema);
	return size - count;
}
ssize_t transdb_pwrite(struct file *filp, char __user *buff, size_t count, loff_t offp) {
	int ret = transdb_write(filp, buff, count, &offp);
	if(ret >= 0) offp -= ret;
	return ret;
}

loff_t transdb_llseek(struct file *filp, loff_t off, int whence) {
	switch(whence) {
		case SEEK_SET:
			filp->f_pos = off;
			break;
		case SEEK_CUR:
			filp->f_pos += off;
			break;
		case SEEK_END:
			return -EINVAL;
	}
	return filp->f_pos;
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
	int bool = 0;
	struct needle *pos;
	struct needle *temp;
	struct db_block *blk;

	up_read(&db_rwsema);
	down_write(&db_rwsema); 
	down(&trans->sema);
	printk(KERN_WARNING "Accept trans begin: %d\n", trans->id);

	list_for_each_entry(pos, &trans->list, list) {
		if((blk = rb_search(&db_root, pos->id)) == NULL) continue;
		if(blk->ts > trans->id) {
			bool = 1;
			break;
		}
	}

	if(bool) {
		up_write(&db_rwsema);
		delete_list_with_lock(trans);
		return -EDEADLK;
	}
	trans->id = 0;

	list_for_each_entry_safe(pos, temp, &trans->list, list) {
		blk = rb_search(&db_root, pos->id);
		if(blk == NULL) {
			blk = kmalloc(sizeof(struct db_block), GFP_KERNEL);
			if(blk == NULL) {
				up_write(&db_rwsema);
				delete_list_with_lock(trans);
				return -ENOMEM;
			}
		 	blk->id = pos->id;
			memset(blk->data, 0, DB_BLOCK_SIZE);
			rb_insert(&db_root, blk);
		}
		blk->ts = db_counter;
		memcpy(blk->data + pos->beg, 
				 pos->data + pos->beg, 
				 pos->end - pos->beg + 1);
		list_del(&pos->list);
		kfree(pos);
	}

	printk(KERN_WARNING "Accept trans end: %d\n", trans->id);
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
	llseek: transdb_llseek,
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
	/* Init read-write semaphore */
	init_rwsem(&db_rwsema);
	/* Init global semaphore */
	sema_init(&db_glsema, 1);
	/* Init read-black tree to store database */
	db_root = RB_ROOT;
	
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
