/*
 * Virtio Cryptodev Device
 *
 * Implementation of virtio-cryptodev qemu backend device.
 *
 * Dimitris Siakavaras <jimsiak@cslab.ece.ntua.gr>
 * Stefanos Gerangelos <sgerag@cslab.ece.ntua.gr> 
 * Konstantinos Papazafeiropoulos <kpapazaf@cslab.ece.ntua.gr>
 *
 */

#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "hw/qdev.h"
#include "hw/virtio/virtio.h"
#include "standard-headers/linux/virtio_ids.h"
#include "hw/virtio/virtio-cryptodev.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
/*
#define VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCGSESSION 3
#define VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCCRYPT 4
#define VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCFSESSION 5
*/

static uint64_t get_features(VirtIODevice *vdev, uint64_t features,
                             Error **errp)
{
    DEBUG_IN();
    return features;
}

static void get_config(VirtIODevice *vdev, uint8_t *config_data)
{
    DEBUG_IN();
}

static void set_config(VirtIODevice *vdev, const uint8_t *config_data)
{
    DEBUG_IN();
}

static void set_status(VirtIODevice *vdev, uint8_t status)
{
    DEBUG_IN();
}

static void vser_reset(VirtIODevice *vdev)
{
    DEBUG_IN();
}

static void vq_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtQueueElement *elem;
    unsigned int *syscall_type;
    unsigned char *key;
    struct session_op *session;
    int *fd_open;
    int *fd_dev,*guest_cmd;
    int *fd_close;
    struct crypt_op * cryp;
    unsigned char *src, *dst, *iv;
    uint32_t *ses_id;
    int *ioctl_to_guest;
                               
    DEBUG_IN();

    elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
    if (!elem) {
        DEBUG("No item to pop from VQ :(");
        return;
    } 

    DEBUG("I have got an item from VQ :)");
    
  
    syscall_type = elem->out_sg[0].iov_base; 
    switch (*syscall_type) {
    case VIRTIO_CRYPTODEV_SYSCALL_TYPE_OPEN:
        DEBUG("VIRTIO_CRYPTODEV_SYSCALL_TYPE_OPEN");
        fd_open = elem->in_sg[0].iov_base; // in_sg[0] -> backend says fd_open is ...
        DEBUG("I AM HERE");
        *fd_open = open(CRYPTODEV_FILENAME,O_RDWR); 
        if(*fd_open < 0){
            DEBUG("LOS POLLOS HERMANOS OPEN");
        }
        printf("Opened /dev/crypto file with fd = %d\n",*fd_open);
        break;

    case VIRTIO_CRYPTODEV_SYSCALL_TYPE_CLOSE:
        DEBUG("VIRTIO_CRYPTODEV_SYSCALL_TYPE_CLOSE");
        fd_close = elem->out_sg[1].iov_base; // out_sg[1] -> guest says close( ... )
        if(close(*fd_close) < 0){
            DEBUG("LOS POLLOS HERMANOS CLOSEed");
        }
        printf("Closed /dev/crypto file with fd = %d\n",*fd_close);
        break;

    case VIRTIO_CRYPTODEV_SYSCALL_TYPE_IOCTL:
        DEBUG("VIRTIO_CRYPTODEV_SYSCALL_TYPE_IOCTL");
        fd_dev = elem->out_sg[1].iov_base; //se poio /dev/crypto milame
        // *fd_dev = open(CRYPTODEV_FILENAME, O_RDWR);
        //thelw guest_cmd apo out_sg[2]
        guest_cmd = elem->out_sg[2].iov_base;
        switch(*guest_cmd){
            case VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCGSESSION:
                DEBUG("Entering CIOCGSESSION");
                key = elem->out_sg[3].iov_base;
                session = elem->in_sg[0].iov_base;
                ioctl_to_guest = elem->in_sg[1].iov_base;   
                session->key = key; //htan me pointer sthn struct
               
                *ioctl_to_guest = ioctl(*fd_dev,CIOCGSESSION,session);
               
                if(*ioctl_to_guest){
                    perror("ioctl(CIOCGSESSION)");
                }            
                DEBUG("Leaving CIOCGSESSION");

                break;
            case VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCCRYPT:
                DEBUG("Entering CIOCCRYPT");

                ioctl_to_guest = elem->in_sg[0].iov_base; //ioctl_to_guest = elem->in_sg[1].iov_base; 
                
                //theloume src, iv, dst gt einai orismena me pointer sth struct  
                cryp = elem->out_sg[3].iov_base;
                src = elem->out_sg[4].iov_base;
                iv = elem->out_sg[5].iov_base;
                dst = elem->in_sg[1].iov_base; //dst = elem->in_sg[0].iov_base;

                cryp->src = src;
                cryp->dst = dst; //isoi pointrs, deixnoun sto idio shmeio mnhmhs
                cryp->iv = iv;

                *ioctl_to_guest = ioctl(*fd_dev,CIOCCRYPT,cryp);
                if(*ioctl_to_guest){
                    perror("ioctl(CIOCCRYPT)");
                }
                
                DEBUG("Leaving CIOCCRYPT");
                break;
            case VIRTIO_CRYPTODEV_SYSCALL_TYPE_CIOCFSESSION:
                DEBUG("Entering CIOCFSESSION");

                ses_id = elem->out_sg[3].iov_base;
                ioctl_to_guest = elem->in_sg[0].iov_base;
            
                *ioctl_to_guest = ioctl(*fd_dev,CIOCFSESSION,ses_id);
                if(*ioctl_to_guest){
                    perror("ioctl(CIOCFSESSION)");
                }
                break;
                DEBUG("Leaving CIOCFSESSION");
        }

        /*unsigned char *output_msg = elem->out_sg[1].iov_base;
        unsigned char *input_msg = elem->in_sg[0].iov_base;
        memcpy(input_msg, "Host: Welcome to the virtio World!", 35);
        printf("Guest says: %s\n", output_msg); 
        printf("We say: %s\n", input_msg);*/
        break;

    default:
        DEBUG("Unknown syscall_type");
        break;
    }

    virtqueue_push(vq, elem, 0);
    virtio_notify(vdev, vq);
    g_free(elem);
}

static void virtio_cryptodev_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);

    DEBUG_IN();

    virtio_init(vdev, "virtio-cryptodev", VIRTIO_ID_CRYPTODEV, 0);
    virtio_add_queue(vdev, 128, vq_handle_output);
}

static void virtio_cryptodev_unrealize(DeviceState *dev, Error **errp)
{
    DEBUG_IN();
}

static Property virtio_cryptodev_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_cryptodev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *k = VIRTIO_DEVICE_CLASS(klass);

    DEBUG_IN();
    dc->props = virtio_cryptodev_properties;
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);

    k->realize = virtio_cryptodev_realize;
    k->unrealize = virtio_cryptodev_unrealize;
    k->get_features = get_features;
    k->get_config = get_config;
    k->set_config = set_config;
    k->set_status = set_status;
    k->reset = vser_reset;
}

static const TypeInfo virtio_cryptodev_info = {
    .name          = TYPE_VIRTIO_CRYPTODEV,
    .parent        = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtCryptodev),
    .class_init    = virtio_cryptodev_class_init,
};

static void virtio_cryptodev_register_types(void)
{
    type_register_static(&virtio_cryptodev_info);
}

type_init(virtio_cryptodev_register_types)
