/*
 * crypto-chrdev.c
 *
 * Implementation of character devices
 * for virtio-cryptodev device 
 **
 * Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
 * Dimitris Siakavaras <jimsiak@cslab.ece.ntua.gr>
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr>
 *
 */
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>

#include "crypto.h"
#include "crypto-chrdev.h"
#include "debug.h"

#include "cryptodev.h"

/*
 * Global data
 */
struct cdev crypto_chrdev_cdev;

/**
 * Given the minor number of the inode return the crypto device 
 * that owns that number.
 **/
static struct crypto_device *get_crypto_dev_by_minor(unsigned int minor)
{
	struct crypto_device *crdev;
	unsigned long flags;

	debug("Entering");

	spin_lock_irqsave(&crdrvdata.lock, flags);
	list_for_each_entry(crdev, &crdrvdata.devs, list) {
		if (crdev->minor == minor)
			goto out;
	}
	crdev = NULL;

out:
	spin_unlock_irqrestore(&crdrvdata.lock, flags);

	debug("Leaving");
	return crdev;
}

/*************************************
 * Implementation of file operations
 * for the Crypto character device
 *************************************/

static int crypto_chrdev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int err;
	unsigned int len;
	struct crypto_open_file *crof;
	struct crypto_device *crdev;
	unsigned int *syscall_type;
	int *host_fd;
	struct scatterlist syscall_type_sg, host_fd_sg, *sgs[2];
	int num_in = 0;
	int num_out = 0;
	unsigned long flags;
	debug("Entering");

	syscall_type = kzalloc(sizeof(*syscall_type), GFP_KERNEL);
	*syscall_type = VIRTIO_CRYPTODEV_SYSCALL_OPEN;
	host_fd = kzalloc(sizeof(*host_fd), GFP_KERNEL);
	*host_fd = -1;

	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto fail;

	/* Associate this open file with the relevant crypto device. */
	crdev = get_crypto_dev_by_minor(iminor(inode)); // afti i sinartisi epistrefi ena crypto_device * kai dev->lock exoume klidaria	
	if (!crdev) {
		debug("Could not find crypto device with %u minor", 
				iminor(inode));
		ret = -ENODEV;
		goto fail;
	}

	crof = kzalloc(sizeof(*crof), GFP_KERNEL);
	if (!crof) {
		ret = -ENOMEM;
		goto fail;
	}
	crof->crdev = crdev;
	crof->host_fd = -1;
	filp->private_data = crof;

	/**
	 * We need two sg lists, one for syscall_type and one to get the 
	 * file descriptor from the host.
	 **/
	//struct scatterlist syscall_type_sg, host_fd_sg;

	sg_init_one(&syscall_type_sg,syscall_type,sizeof(*syscall_type));
	sgs[num_out++] = &syscall_type_sg;
	sg_init_one(&host_fd_sg,&crof->host_fd,sizeof(crof->host_fd));
	sgs[num_out + num_in++] = &host_fd_sg;
	/**
	 * Wait for the host to process our data.
	 **/
	spin_lock_irqsave(&crdev->lock, flags); //semaphore gia parallhlia ulikou

	virtqueue_add_sgs(crdev->vq,sgs,num_out,num_in,&syscall_type_sg,GFP_ATOMIC);//GFP_ATOMIC:Allocation will not sleep.Use this
	virtqueue_kick(crdev->vq);					 	      //inside interrupt handlers

	while(virtqueue_get_buf(crdev->vq, &len) == NULL) ;
	spin_unlock_irqrestore(&crdev->lock, flags);

	//while(virtqueue_get_buf(crdev->vq, &len) ==  NULL)	
	//crof->host_fd = *host_fd;
	if(crof->host_fd < 0){
		debug("Host failed to open crypto device");
		ret = -ENODEV;
	}	

fail:
	debug("Leaving");
	return ret;
}

static int crypto_chrdev_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct crypto_open_file *crof = filp->private_data;
	struct crypto_device *crdev = crof->crdev;
	unsigned int *syscall_type;
	struct scatterlist syscall_type_sg,host_fd_sg, *sgs[2];
	unsigned int num_in, num_out, len;
	unsigned long flags;
	num_in = 0;
	num_out = 0;
	debug("Entering");

	syscall_type = kzalloc(sizeof(*syscall_type), GFP_KERNEL);
	*syscall_type = VIRTIO_CRYPTODEV_SYSCALL_CLOSE;

	sg_init_one(&syscall_type_sg, syscall_type, sizeof(syscall_type));
	sgs[num_out++] = &syscall_type_sg;
	sg_init_one(&host_fd_sg, &crof->host_fd,sizeof(crof->host_fd));
	sgs[num_out++] = &host_fd_sg;
	/**
	 * Send data to the host.
	 **/
	printk(KERN_INFO "%d\n", crof->host_fd);
	spin_lock_irqsave(&crdev->lock, flags);

	virtqueue_add_sgs(crdev->vq, sgs, num_out, num_in, &syscall_type_sg, GFP_ATOMIC);
	virtqueue_kick(crdev->vq);

	/*here*/
	while(virtqueue_get_buf(crdev->vq, &len) == NULL);

	spin_unlock_irqrestore(&crdev->lock, flags);


	/**
	 * Wait for the host to process our data.
	 **/ /*there*/


	kfree(crof);
	debug("Leaving");
	return ret;

}

static long crypto_chrdev_ioctl(struct file *filp, unsigned int cmd, 
		unsigned long arg)
{
	long ret = 0;
	int err;
	struct crypto_open_file *crof = filp->private_data;
	struct crypto_device *crdev = crof->crdev;
	struct virtqueue *vq = crdev->vq;
	struct scatterlist output_msg_sg,input_msg_sg,syscall_type_sg, host_fd_sg, 
			   ioctl_cmd_sg, sess_sg, key_sg, host_ret_sg,
			   iv_sg, src_sg, cryp_sg, dst_sg,
			   *sgs[8];
	unsigned int num_out, num_in, len;
#define MSG_LEN 100
	unsigned char *output_msg, *input_msg, *key, *dst = NULL,*iv = NULL, *src = NULL;
	unsigned int *syscall_type;
	unsigned int *ioctl_cmd;
	long *host_ret = kmalloc(sizeof(*host_ret), GFP_KERNEL);
	struct session_op *sess;
	uint32_t *ses = NULL;
	struct crypt_op *cryp = NULL;
	unsigned long flags;

	debug("Entering");

	/**
	 * Allocate all data that will be sent to the host.
	 **/
	output_msg = kzalloc(MSG_LEN, GFP_KERNEL);
	input_msg = kzalloc(MSG_LEN, GFP_KERNEL);
	syscall_type = kzalloc(sizeof(*syscall_type), GFP_KERNEL);
	*syscall_type = VIRTIO_CRYPTODEV_SYSCALL_IOCTL;

	num_out = 0;
	num_in = 0;

	/**
	 *  These are common to all ioctl commands.
	 **/
	sg_init_one(&syscall_type_sg, syscall_type, sizeof(*syscall_type));
	sg_init_one(&host_fd_sg, &crof->host_fd, sizeof(crof->host_fd));

	sgs[num_out++] = &syscall_type_sg;
	sgs[num_out++] = &host_fd_sg;


	/**
	 *  Add all the cmd specific sg lists.
	 **/
	switch (cmd) {
		case CIOCGSESSION:
			debug("CIOCGSESSION");
			memcpy(output_msg, "Hello HOST from ioctl CIOCGSESSION.", 36);
			input_msg[0] = '\0';
			/*sg_init_one(&output_msg_sg, output_msg, MSG_LEN);
			  sgs[num_out++] = &output_msg_sg;
			  sg_init_one(&input_msg_sg, input_msg, MSG_LEN);
			  sgs[num_out + num_in++] = &input_msg_sg;
			  */

			ioctl_cmd = kzalloc(sizeof(*ioctl_cmd), GFP_KERNEL);
			*ioctl_cmd = VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCGSESSION;

			//sgs[num_out++] = &ioctl_cmd;

			sess = kzalloc(sizeof(*sess), GFP_KERNEL);
			if (copy_from_user(sess, (struct session_op *)arg, sizeof(*sess) )) {
				debug("Error copying session");
				ret = -EFAULT;
				goto out;
			}

			//sess->key => 
			key = kzalloc(sess->keylen, GFP_KERNEL);
			if (copy_from_user(key, sess->key, sess->keylen)) {
				debug("Copy key from user failed");
				ret = -EFAULT;
				goto out;
			}

			/* data sent out_sg[2] = cmd & out_sg[3] = key */
			sg_init_one(&ioctl_cmd_sg, ioctl_cmd, sizeof(*ioctl_cmd));
			sgs[num_out++] = &ioctl_cmd_sg;

			sg_init_one(&key_sg, key, sizeof(*key));
			sgs[num_out++] = &key_sg;

			/* data received in_sg[0] & in_sg[1] */
			sg_init_one(&sess_sg, sess,sizeof(*sess));
			sgs[num_out + num_in++] = &sess_sg;

			sg_init_one(&host_ret_sg, host_ret, sizeof(*host_ret));             //in_sg[1]
			sgs[num_out + num_in++] = &host_ret_sg;

			break;

		case CIOCFSESSION:
			debug("CIOCFSESSION");
			/*
			   memcpy(output_msg, "Hello HOST from ioctl CIOCFSESSION.", 36);
			   input_msg[0] = '\0';
			   sg_init_one(&output_msg_sg, output_msg, MSG_LEN);
			   sgs[num_out++] = &output_msg_sg;
			   sg_init_one(&input_msg_sg, input_msg, MSG_LEN);
			   sgs[num_out + num_in++] = &input_msg_sg;
			   */

			ioctl_cmd = kzalloc(sizeof(*ioctl_cmd), GFP_KERNEL);
			*ioctl_cmd = VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCFSESSION;

			ses = kmalloc(sizeof(uint32_t), GFP_KERNEL);

			if (copy_from_user(ses, (uint32_t *)arg, sizeof(*ses))) {
				debug("Copy from user failed");
				ret = -EFAULT;
				goto out;
			}

			sg_init_one(&ioctl_cmd_sg, ioctl_cmd, sizeof(*ioctl_cmd));       //out_sg[2]
			sgs[num_out++] = &ioctl_cmd_sg;
			sg_init_one(&sess_sg, ses, sizeof(*ses));                            //out_sg[3]
			sgs[num_out++] = &sess_sg;

			sg_init_one(&host_ret_sg, host_ret, sizeof(*host_ret));             //in_sg[0]
			sgs[num_out + num_in++] = &host_ret_sg;

			break;

		case CIOCCRYPT:
			debug("CIOCCRYPT");

			//define ioctl command
			ioctl_cmd = kzalloc(sizeof(*ioctl_cmd), GFP_KERNEL);
			*ioctl_cmd = VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCCRYPT;

			cryp = kmalloc(sizeof(*cryp), GFP_KERNEL);
			if (copy_from_user(cryp, (struct cryp_op*)arg, sizeof(*cryp))) {
				debug("Copy from user failed");
				ret = -EFAULT;
				goto out;
			}

			src = kzalloc(cryp->len, GFP_KERNEL);
			if (copy_from_user(src, cryp->src, cryp->len)) {
				debug("Copy src from user failed");
				ret = -EFAULT;
				goto out;
			}

			iv = kzalloc(EALG_MAX_BLOCK_LEN, GFP_KERNEL);
			if (copy_from_user(iv, cryp->iv, EALG_MAX_BLOCK_LEN)) {
				debug("Copy iv from user failed");
				ret = -EFAULT;
				goto out;
			}	

			dst = kzalloc(cryp->len,GFP_KERNEL);

			/* 			data sent  			 */
			sg_init_one(&ioctl_cmd_sg, ioctl_cmd, sizeof(*ioctl_cmd));       
			sgs[num_out++] = &ioctl_cmd_sg;

			sg_init_one(&cryp_sg, cryp, sizeof(*cryp));                         //out_sg[3]
			sgs[num_out++] = &cryp_sg;

			sg_init_one(&src_sg, src, cryp->len);                               //out_sg[4]
			sgs[num_out++] = &src_sg;

			sg_init_one(&iv_sg, iv, EALG_MAX_BLOCK_LEN);                        //out_sg[5]
			sgs[num_out++] = &iv_sg;

			/*		data received  			*/

			
			
			sg_init_one(&host_ret_sg, host_ret, sizeof(*host_ret));
			sg_init_one(&dst_sg, dst, sizeof(*dst));
			
			sgs[num_out + num_in++] = &host_ret_sg;
			sgs[num_out + num_in++] = &dst_sg;
			//sgs[num_out+ num_in++] = &host_ret_sg;
			break;

		default:
			debug("Unsupported ioctl command");

			break;
	}

	spin_lock_irqsave(&crdev->lock, flags);

	virtqueue_add_sgs(vq, sgs, num_out, num_in, &syscall_type_sg, GFP_ATOMIC);
	virtqueue_kick(vq);
	while (virtqueue_get_buf(vq, &len) == NULL);

	spin_unlock_irqrestore(&crdev->lock, flags);


	switch (cmd) {
		case CIOCGSESSION:
			//debug("CIOCGSESSION IN IOCTL",
			/* restore the pointer sess->key to original userspace address */
			sess->key = ((struct session_op *) arg)->key;

			/*        and return the session to the userspac               */
			if (copy_to_user((struct session_op *) arg, sess, sizeof(*sess))) {
				debug("Copy to user failed!");
				ret = -EFAULT;
				goto out;
			}
			break;

		case CIOCFSESSION:
			/*
			   if (copy_to_user((uint32_t *)arg, ses, sizeof(*ses))) {
			   debug("Copy to user failed!");
			   ret = -EFAULT;
			   goto out;
			   }*/
			//kfree(ses);
			break;

		case CIOCCRYPT:
			//return dst data (encrypted/decrypted back to userspace)
			if (copy_to_user(((struct crypt_op *)arg)->dst, dst, cryp->len)) {
				debug("Copy to user failed!");
				ret = -EFAULT;
				goto out;
			}
			break;

		default:
			debug("Unsupported ioctl command (2nd)");
			break;
	}

	ret = *host_ret;
out:

	/*
	kfree(cryp);
	kfree(dst);
	kfree(src);
	kfree(iv);
	kfree(ioctl_cmd);
	kfree(ses);
	kfree(sess);
	kfree(key);
	kfree(output_msg);
	kfree(input_msg);
	kfree(syscall_type);
	kfree(host_ret);
	*/

	return ret;
}

static ssize_t crypto_chrdev_read(struct file *filp, char __user *usrbuf, 
		size_t cnt, loff_t *f_pos)
{
	debug("Entering");
	debug("Leaving");
	return -EINVAL;
}

static struct file_operations crypto_chrdev_fops = 
{
	.owner          = THIS_MODULE,
	.open           = crypto_chrdev_open,
	.release        = crypto_chrdev_release,
	.read           = crypto_chrdev_read,
	.unlocked_ioctl = crypto_chrdev_ioctl,
};

int crypto_chrdev_init(void)
{
	int ret;
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;

	debug("Initializing character device...");
	cdev_init(&crypto_chrdev_cdev, &crypto_chrdev_fops);
	crypto_chrdev_cdev.owner = THIS_MODULE;

	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	ret = register_chrdev_region(dev_no, crypto_minor_cnt, "crypto_devs");
	if (ret < 0) {
		debug("failed to register region, ret = %d", ret);
		goto out;
	}
	ret = cdev_add(&crypto_chrdev_cdev, dev_no, crypto_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device");
		goto out_with_chrdev_region;
	}

	debug("Completed successfully");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
out:
	return ret;
}

void crypto_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int crypto_minor_cnt = CRYPTO_NR_DEVICES;

	debug("entering");
	dev_no = MKDEV(CRYPTO_CHRDEV_MAJOR, 0);
	cdev_del(&crypto_chrdev_cdev);
	unregister_chrdev_region(dev_no, crypto_minor_cnt);
	debug("leaving");
}

