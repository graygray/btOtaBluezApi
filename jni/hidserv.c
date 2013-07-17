#include "hidserv.h"
#include "utils/Log.h"//Tina add



#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>


#include <signal.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/hidp.h>

#include "include/OTADefine.h"
#include "include/log.h"
#include "sdp.h"


#define TAG "====OTAUpdate===="

//#define VendorID 		"23a1"		// Morvell BT Remote Control Vendor ID
//#define ProductID		"1234"		// Morvell BT Remote Control Product ID


// Returned Error Code
#define ERR_ARGS 			1
#define ERR_OPEN 			2
#define ERR_HIDIOCGRAWNAME 	3
#define ERR_HIDIOCGRAWPHYS 	4
#define ERR_HIDIOCGRAWINFO 	5
#define ERR_WRITE 			6

// global variable
pthread_mutex_t t_mutex;
pthread_cond_t t_cond;
static int num1 = 0;
static int num2 = 0;
static int fd = -1;
static int flagRead;
static int OTAStatus;
static int OTAStage;
//static int data_total_pkg_num;				// number of data packets needed
static int current_data_seq;				// sequence of data packets
static int isFinishCmd;
static int isVerboseLog;
static int total_cmd_num;					// count number of command

static char start_cmd[start_cmd_len];
static char data_cmd[data_cmd_len];
static char finish_cmd[finish_cmd_len];
static char rsp_cmd[rsp_cmd_len];



uint8_t cls[3];

int	 ctrl,\
	 intr,\
	 csg,\
	 isg,\
	 class_st=0,\
	 connection_status =0;

char default_class[8];
pthread_t thread;

static int request_authentication(bdaddr_t *src, bdaddr_t *dst)
{
	struct hci_conn_info_req *cr;
	char addr[18];
	int err, dd, dev_id;

	ba2str(src, addr);
	dev_id = hci_devid(addr);
	if (dev_id < 0)
		return dev_id;

	dd = hci_open_dev(dev_id);
	if (dd < 0)
		return dd;

	cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
	if (!cr)
		return -ENOMEM;

	bacpy(&cr->bdaddr, dst);
	cr->type = ACL_LINK;
	err = ioctl(dd, HCIGETCONNINFO, (unsigned long) cr);
	if (err < 0) {
		free(cr);
		hci_close_dev(dd);
		return err;
	}

	err = hci_authenticate_link(dd, htobs(cr->conn_info->handle), 25000);

	free(cr);
	hci_close_dev(dd);

	return err;
}

static int request_encryption(bdaddr_t *src, bdaddr_t *dst)
{
	struct hci_conn_info_req *cr;
	char addr[18];
	int err, dd, dev_id;

	ba2str(src, addr);
	dev_id = hci_devid(addr);
	if (dev_id < 0)
		return dev_id;

	dd = hci_open_dev(dev_id);
	if (dd < 0)
		return dd;

	cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
	if (!cr)
		return -ENOMEM;

	bacpy(&cr->bdaddr, dst);
	cr->type = ACL_LINK;
	err = ioctl(dd, HCIGETCONNINFO, (unsigned long) cr);
	if (err < 0) {
		free(cr);
		hci_close_dev(dd);
		return err;
	}

	err = hci_encrypt_link(dd, htobs(cr->conn_info->handle), 1, 25000);

	free(cr);
	hci_close_dev(dd);

	return err;
}

static int enable_sixaxis(int csk)
{
	const unsigned char buf[] = {
		0x53 /*HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_FEATURE*/,
		0xf4,  0x42, 0x03, 0x00, 0x00 };

	return write(csk, buf, sizeof(buf));
}


//Don't execute (Permission Denied)
static int create_device(int ctl, int csk, int isk, uint8_t subclass, int nosdp, int nocheck, int bootonly, int encrypt, int timeout)
{
	struct hidp_connadd_req req;
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	bdaddr_t src, dst;
	char bda[18];
	int err;
	LOGE("============Tina==================== create_device 1");
	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if (getsockname(csk, (struct sockaddr *) &addr, &addrlen) < 0)
		return -1;

	bacpy(&src, &addr.l2_bdaddr);

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if (getpeername(csk, (struct sockaddr *) &addr, &addrlen) < 0)
		return -1;

	bacpy(&dst, &addr.l2_bdaddr);

	memset(&req, 0, sizeof(req));
	req.ctrl_sock = csk;
	req.intr_sock = isk;
	req.flags     = 0;
	req.idle_to   = timeout * 60;

	err = get_stored_device_info(&src, &dst, &req);
	if (!err)
		goto create;

	LOGE("============Tina==================== create_device 2");
	if (!nocheck) {
		ba2str(&dst, bda);
		LOGE("Rejected connection from unknown device %s", bda);
		/* Return no error to avoid run_server() complaining too */
		return 0;
	}

	if (!nosdp) {
		err = get_sdp_device_info(&src, &dst, &req);
		if (err < 0)
			goto error;
	} else {
		struct l2cap_conninfo conn;
		socklen_t size;
		uint8_t class[3];

		memset(&conn, 0, sizeof(conn));
		size = sizeof(conn);
		if (getsockopt(csk, SOL_L2CAP, L2CAP_CONNINFO, &conn, &size) < 0)
			memset(class, 0, 3);
		else
			memcpy(class, conn.dev_class, 3);

		if (class[1] == 0x25 && (class[2] == 0x00 || class[2] == 0x01))
			req.subclass = class[0];
		else
			req.subclass = 0xc0;
	}

create:
	LOGE("============Tina==================== create_device 3");
	if (subclass != 0x00)
		req.subclass = subclass;

	ba2str(&dst, bda);
	LOGV("New HID device %s (%s)", bda, req.name);

	if (encrypt && (req.subclass & 0x40)) {
		LOGE("============Tina==================== create_device 4");
		err = request_authentication(&src, &dst);
		if (err < 0) {
			LOGE("Authentication for %s failed", bda);
			goto error;
		}

		err = request_encryption(&src, &dst);
		if (err < 0)
			LOGE("Encryption for %s failed", bda);
	}

	if (bootonly) {
		req.rd_size = 0;
		req.flags |= (1 << HIDP_BOOT_PROTOCOL_MODE);
	}

	if (req.vendor == 0x054c && req.product == 0x0268)
		enable_sixaxis(csk);

	err = ioctl(ctl, HIDPCONNADD, &req);
	LOGE("HIDPCONNADD  err:%d    ERR:%s", err, strerror(errno));

error:
	free(req.rd_data);

	return err;
}





int l2cap_connect(bdaddr_t *src, bdaddr_t *dst, unsigned short psm)
{
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	int sk;

	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect 1\n");
	if ((sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0){
		LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect err 1\n");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.l2_family  = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, src);

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect err 2\n");
		close(sk);
		return -1;
	}
	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect 2\n");

	memset(&opts, 0, sizeof(opts));
	opts.imtu = HIDP_DEFAULT_MTU;
	opts.omtu = HIDP_DEFAULT_MTU;
	opts.flush_to = 0xffff;

	setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts));

	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect 3\n");
	memset(&addr, 0, sizeof(addr));
	addr.l2_family  = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, dst);
	addr.l2_psm = htobs(psm);

	if (connect(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect err 3   ERR:%s\n", strerror(errno));
		close(sk);
		return -1;
	}
	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_connect 4  sk:%d\n", sk);

	return sk;
}

/*From Bluez Utils 3.2*/
static int l2cap_listen(const bdaddr_t *bdaddr, unsigned short psm, int lm, int backlog)
{
	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_listen 0");
	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_listen 1  bdaddr::%x:%x:%x:%x:%x:%x    psm:%d\n",bdaddr->b[0],bdaddr->b[1],bdaddr->b[2],bdaddr->b[3],bdaddr->b[4],bdaddr->b[5], psm);
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	int sk;

	if ((sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, bdaddr);
	addr.l2_psm = htobs(psm);

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina  l2cap_listen  bind   psm:%d  ERR:%s\n",psm, strerror(errno));
		close(sk);
		return -1;
	}

	setsockopt(sk, SOL_L2CAP, L2CAP_LM, &lm, sizeof(lm));

	memset(&opts, 0, sizeof(opts));
	opts.imtu = HIDP_DEFAULT_MTU;
	opts.omtu = HIDP_DEFAULT_MTU;
	opts.flush_to = 0xffff;

	setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts));

	if (listen(sk, backlog) < 0) {
		__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina  l2cap_listen  listen   psm:%d  ERR:%s\n",psm, strerror(errno));
		close(sk);
		return -1;
	}

	return sk;
}

/*From Bluez Utils 3.2*/
static int l2cap_accept(int sk, bdaddr_t *bdaddr)
{
	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_accept 0");
	LOGE("@@@@####Tina####@@@@ hidserv.c   l2cap_accept 1 sk:%d\n", sk);
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	int nsk;

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if ((nsk = accept(sk, (struct sockaddr *) &addr, &addrlen)) < 0)
		return -1;

	if (bdaddr)
		bacpy(bdaddr, &addr.l2_bdaddr);

	return nsk;
}

static uint8_t* get_device_class(int hdev)
{
	int s = hci_open_dev(hdev);

	if (s < 0) {
		fprintf(stderr, "Can't open device hci%d: %s (%d)\n",
						hdev, strerror(errno), errno);
		//exit(1);
		return 0;
	}
	
	if (hci_read_class_of_dev(s, cls, 1000) < 0) {
		fprintf(stderr, "Can't read class of device on hci%d: %s (%d)\n",
						hdev, strerror(errno), errno);
		//exit(1);
		return 0;
	}
	
	return cls;

}

static void set_device_class(int hdev, char* class)
{

	int s = hci_open_dev(hdev);

	uint32_t cod = strtoul(class, NULL, 16);
	if (hci_write_class_of_dev(s, cod, 2000) < 0) {
		fprintf(stderr, "Can't write local class of device on hci%d: %s (%d)\n",
						hdev, strerror(errno), errno);
		//exit(1);
	}
}

static int do_connect(int ctl, bdaddr_t *src, bdaddr_t *dst, uint8_t subclass, int fakehid, int bootonly, int encrypt, int timeout)
{
	//src:00:00:00:00:00:00    dst:75:E7:8F:D9:A8:70   subclass:0  fakehid:1  bootonly:0  encrypt:0  timeout:30
	struct hidp_connadd_req req;
	uint16_t uuid = HID_SVCLASS_ID;
	uint8_t channel = 0;
	char name[256];
	char buff[256];
	int csk, isk, err;

	memset(&req, 0, sizeof(req));
	name[0] = '\0';

#if 0 // Tina call exe for premission defined
	char srcaddr[18], dstaddr[18];
	ba2str(src, srcaddr);
	ba2str(dst, dstaddr);
	sprintf(buff, "addcon %d %s %s %d", 1, srcaddr, dstaddr, fakehid);
	LOGE("============Tina==================== do_connect 1  buff:%s",buff);
	system(buff);
#else
	// Don't execute (Permission Denied)
	/*err = get_sdp_device_info(src, dst, &req);
	if (err < 0 && fakehid)
		err = get_alternate_device_info(src, dst,
				&uuid, &channel, name, sizeof(name) - 1);

	if (err < 0) {
		LOGE("Can't get device information");
		close(ctl);
		//exit(1);
	}*/
#endif

	switch (uuid) {
		case HID_SVCLASS_ID:
			goto connect;
	}

	return -1;

connect:
	csk = l2cap_connect(src, dst, L2CAP_PSM_HIDP_CTRL);
	if (csk < 0) {
		LOGE("Can't create HID control channel");
		close(ctl);
		//exit(1);
		return 0;
	}

	isk = l2cap_connect(src, dst, L2CAP_PSM_HIDP_INTR);
	if (isk < 0) {
		LOGE("Can't create HID interrupt channel");
		close(csk);
		close(ctl);
		//exit(1);
		return 0;
	}
	ctrl = csk;
	intr = isk;

#if 0	// Tina call exe for premission defined
	sprintf(buff, "addcon %d %x %x %x", 2,ctl,csk,isk);
	system(buff);
#else
	// Don't execute (Permission Denied)
	/*err = create_device(ctl, csk, isk, subclass, 1, 1, bootonly, encrypt, timeout);
	if (err < 0) {
		LOGE("HID create error %d (%s)\n",
						errno, strerror(errno));
		//close(isk);
		//sleep(1);
		//close(csk);
		//close(ctl);
		//exit(1);
	}*/
#endif
	
	connection_status = 1;
	return 0;
}

int reconnect(char *src, char *dst)
{
	int csk, isk;
	bdaddr_t srcaddr, dstaddr;
	//src = "00:22:F4:11:22:33";

	str2ba(src, &srcaddr);
	str2ba(dst, &dstaddr);
	LOGE("@@@@####Tina####@@@@ hidserv.c   reconnect 1  src:%s    dst:%s\n", src, dst);

	int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HIDP);
	if (ctl < 0) {
		LOGE("Can't open HIDP control socket");
		//exit(1);
		return 0;
	}
	int ret = do_connect(ctl, &srcaddr, &dstaddr, 0, 1, 0, 0, 30);
	LOGE("@@@@####Tina####@@@@ hidserv.c   reconnect 2  ret:%d\n", ret);
	
	return ret;
}


void init_cmd () {

	// initial start command
	start_cmd[f_RID] = SystemFirmware;
	start_cmd[f_CMD] = START_CMD;
	start_cmd[f_s_FS0] = 0x00;
	start_cmd[f_s_FS1] = 0x00;
	start_cmd[f_s_FS2] = 0x00;
	start_cmd[f_s_FS3] = 0x00;
	start_cmd[f_s_RSA0] = 0x00;
	start_cmd[f_s_RSA1] = 0x00;
	start_cmd[f_s_RSA2] = 0x00;
	start_cmd[f_s_RSA3] = 0x00;
	start_cmd[f_s_EAP0] = 0x00;
	start_cmd[f_s_EAP1] = 0x00;
	start_cmd[f_s_EAP2] = 0x00;
	start_cmd[f_s_EAP3] = 0x00;
	start_cmd[f_s_CRCTP] = CRC16CCITT;

	// initial data command
	data_cmd[f_RID] = SystemFirmware;
	data_cmd[f_CMD] = DATA_CMD;
	data_cmd[f_d_SEQ0] = 0x00;
	data_cmd[f_d_SEQ1] = 0x00;

	// initial finish command
	finish_cmd[f_RID] = SystemFirmware;
	finish_cmd[f_CMD] = FINISH_CMD;
}

unsigned short crc16_ccitt(const void *buf, int len)
{
	register int counter;
	register unsigned short crc = 0;
	char *buf2 = (char *)buf;
	for( counter = 0; counter < len; counter++)
		crc = (crc<<8) ^ crc16tab[((crc>>8) ^ *(char *)buf2++)&0x00FF];
	return crc;
}

uint32_t crc_table[256];
/* Run this function previously */
void make_crc_table(void) {
	int i,j;
    for (i = 0; i < 256; i++) {
        int c = i;
        for (j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
}

uint32_t crc32(uint8_t *buf, size_t len) {
    uint32_t c = 0xFFFFFFFF;
    int i;
    for (i = 0; i < len; i++) {
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}

// HID read thread
void *hidRead(void* readData) {

	//int fd;
	int res, index;
	int rsp_seq;							// response sequence no. of data command
	char buf[512];
	char tmp[64];
//	char dev_path[256] = "/dev/hidraw0";
//	char dev_path[256] = "/dev/input/event4";

//	pthread_mutex_lock(&t_mutex);

	// Open the Device
	//fd = open(dev_path, O_RDONLY);

	/*if (fd < 0) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "Error = %d, Unable to open device (read thread)\n", errno);
		__android_log_print(ANDROID_LOG_INFO, TAG, "Unable to open device");
		goto FLAG_OTA_FAIL;
		//return NULL;
	}*/

	while (flagRead) {
		memset(buf, 0x0, sizeof(buf));
		//res = read(fd, buf, sizeof(buf));
		res = read(intr, buf, sizeof(buf));
		if (res < 0 ) {
			__android_log_print(ANDROID_LOG_INFO, TAG, "Error: %d, res:%d  ERR:%s\n", errno, res, strerror(errno));
			__android_log_print(ANDROID_LOG_INFO, TAG, "read");
			/*close(fd);
			return NULL;*/
			goto FLAG_OTA_FAIL;
		} else {
			/*if(isVerboseLog){
				__android_log_print(ANDROID_LOG_INFO, TAG, "=============================read %d bytes at %s \n", res, dev_path);
			}*/
			char tmpWrite[rsp_cmd_len*5], tmp2[10];
			memset(tmp2, 0x0, sizeof(tmp2));
			memset(tmpWrite, 0x0, sizeof(tmpWrite));
			for (index = 0; index < rsp_cmd_len; ++index) {
				if(isVerboseLog){
					sprintf(tmp2, "0x%02X ", buf[index+1]);
					strcat(tmpWrite, tmp2);
				}
				rsp_cmd[index] = buf[index+1];
			}
			if(isVerboseLog){
				__android_log_print(ANDROID_LOG_INFO, TAG, "@@@@:%s\n", tmpWrite);
			}
		}

		if (rsp_cmd[f_RID] == 0x02) {						// check for right RID

			OTAStatus = rsp_cmd[f_r_STAT];
			if(rsp_cmd[f_r_CMD] == START_CMD ) {

				if (OTAStatus == SUCCESS) {
					OTAStage = STAGE_NEXT_CMD;
				} else if ( OTAStatus == NO_NEED_UPDATE) {
					__android_log_print(ANDROID_LOG_INFO, TAG, "NO_NEED_UPDATE\n");
					OTAStage = STAGE_COMPLETE;
				} else {
					__android_log_print(ANDROID_LOG_INFO, TAG, "status error:0x%X, retry\n", rsp_cmd[f_r_STAT]);
					OTAStage = STAGE_ERR_RETRY;
				}

			} else if (rsp_cmd[f_r_CMD] == DATA_CMD) {

				rsp_seq = rsp_cmd[f_r_SEQ0] | (rsp_cmd[f_r_SEQ1] << 8);
				if(isVerboseLog){
					__android_log_print(ANDROID_LOG_INFO, TAG, "response data sequence no. 0x%X\n", rsp_seq);
				}
				if (rsp_seq == current_data_seq) {			// check response sequence is right

					if (OTAStatus == SUCCESS) {
						OTAStage = STAGE_NEXT_CMD;
					} else if ( OTAStatus == NO_NEED_UPDATE) {
						__android_log_print(ANDROID_LOG_INFO, TAG, "NO_NEED_UPDATE\n");
						OTAStage = STAGE_COMPLETE;
					} else {
						__android_log_print(ANDROID_LOG_INFO, TAG, "status error:0x%X, retry\n", rsp_cmd[f_r_STAT]);
						OTAStage = STAGE_ERR_RETRY;
					}

				} else {
					__android_log_print(ANDROID_LOG_INFO, TAG, "sequence no. not match, retry\n");
					OTAStage = STAGE_ERR_RETRY;
				}

			} else if (rsp_cmd[f_r_CMD] == FINISH_CMD) {

				if (OTAStatus == SUCCESS) {
					OTAStage = STAGE_COMPLETE;
				} else if ( OTAStatus == NO_NEED_UPDATE) {
					__android_log_print(ANDROID_LOG_INFO, TAG, "NO_NEED_UPDATE\n");
					OTAStage = STAGE_COMPLETE;
				} else {
					__android_log_print(ANDROID_LOG_INFO, TAG, "status error:0x%X, retry\n", rsp_cmd[f_r_STAT]);
					OTAStage = STAGE_ERR_RETRY;
				}
			}

		}
		// check hid input(OTA response) every 200ms
		usleep(10000);		// 10ms

	}
	return NULL;

FLAG_OTA_FAIL:
	flagRead = 0;
	__android_log_print(ANDROID_LOG_INFO, TAG, "OTA fail ~~======================================\n");
	__android_log_print(ANDROID_LOG_INFO, TAG, "OTA STOP @@======================================\n");
	//close(fd);
	return NULL;
}

int hidWrite(char* writeData, int data_length) {

	//int fd;
	int i, res;
	//char tmp[64];
//	char dev_path[256] = "/dev/hidraw0";
//	char dev_path[256] = "/dev/input/event4";

	/* Open the Device with non-blocking reads. In real life,
	   don't use a hard coded path; use libudev instead. */
	//fd = open(dev_path, O_RDWR | O_NONBLOCK);

	/*if (fd < 0) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "Error = %d, Unable to open device\n", errno);
		__android_log_print(ANDROID_LOG_INFO, TAG, "Unable to open device");
		return -ERR_OPEN;
	}*/

	if(isVerboseLog){
		//__android_log_print(ANDROID_LOG_INFO, TAG, "write dev_path:%s, data_length:%d \nwrite data = ", dev_path, data_length);
		char tmpWrite[data_length*3], tmp2[10];
		memset(tmp2, 0x0, sizeof(tmp2));
		memset(tmpWrite, 0x0, sizeof(tmpWrite));
		for (i = 0; i < data_length ; i++) {
			sprintf(tmp2, "%02X ", writeData[i]);
			strcat(tmpWrite, tmp2);
		}
		__android_log_print(ANDROID_LOG_INFO, TAG, "@@@@:%s", tmpWrite);
	}


	//res = write(fd, writeData, data_length);
	res = write(intr, writeData, data_length);
	if (res < 0) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "Error: %d, write bytes error   ERR:%s\n", errno, strerror(errno));
		__android_log_print(ANDROID_LOG_INFO, TAG, "write");
		return -ERR_WRITE;
	} else {
		if(isVerboseLog){
			__android_log_print(ANDROID_LOG_INFO, TAG, "write %d bytes successfully\n", res);
		}
	}
	fflush(stdout);
	return 0;
}


// Do OTA Firmware Update
int ota_update(char* param1, char* param2) {
	int i = 0;
	unsigned char c;
	int ret = 0;
	int fw_data_len = 0;
	int remain_data_len = 0;
	int wait_count = 0;
	int send_count = 0;
	int current_cmd_idx = 0;			// current command length
	int current_cmd_len = 0;			// current command length
	unsigned char fw_data_buf[524288];	// 2^19 =~ 512 k
	char cmd_buf[1024];
	char tmp[64];
	char *fileName;
	uint16_t CRCRes = 0;
	pthread_t readThread;

	// init vars
	num1 = 0;
	num2 = 0;
	flagRead = 1;
	isFinishCmd = 0;
	isVerboseLog = 0;
	total_cmd_num = 0;

	if (param1 !=NULL) {
		fileName =param1;
	}

	if (param2 != NULL) {
		if ( !strcmp(param2, "Y") || !strcmp(param2, "y") ) {
			isVerboseLog = 1;
		} else {
			isVerboseLog = 0;
		}
	}
	
	if(isVerboseLog){
		__android_log_print(ANDROID_LOG_INFO, TAG, "  ota_update 000001 %s %d",fileName,isVerboseLog);
	}
	
	// open & read fw update data
	FILE *fp;
	//fp = fopen("/mnt/sdcard/Marvell_OTA_Test_BinFile/63MIR.img", "rb");//////////////////////////////////////////////////////////////////////////////// OTA File Path
	fp = fopen(fileName, "rb");
	if (!fp) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "open fw file fail: %s\n",strerror(errno));
		//fclose(fp);
		/*close(fd);
		return -1;*/
		goto FLAG_OTA_FAIL;
	}

	i = 0;
	c = fgetc(fp);
	while (!feof(fp)) {
//		printf("%02X ", c);		// print file content
		fw_data_buf[i] = c;		// read to buffer
		i++;
		c = fgetc(fp);
	}
	fw_data_len = i;
//	printf("\n");
	remain_data_len = fw_data_len;
	__android_log_print(ANDROID_LOG_INFO, TAG, "update FW size:%d\n", fw_data_len);
	fclose(fp);

	// create read thread
	ret = pthread_create(&readThread, NULL, hidRead, NULL);
	if (ret != 0) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "pthread_create fail, ret:%d\n", ret);
	}


FLAG_SNED_NEXT_CMD:

	// init var
	send_count = 0;
	CRCRes = 0;
	current_data_seq = 0;
	memset(cmd_buf, 0x00, sizeof(cmd_buf));
	OTAStage= STAGE_NEW_CMD;

	total_cmd_num++;
//	printf("NO. %d of total cmd send\n", total_cmd_num);
	current_cmd_idx += current_cmd_len;
//	printf("current_cmd_idx:%d\n", current_cmd_idx);

	// length include HDR & SIZE field
	current_cmd_len = fw_data_buf[current_cmd_idx + 1] | ( fw_data_buf[current_cmd_idx + 2] << 8 );
//	printf("current_cmd_len:%d\n", current_cmd_len);

	// fill command buffer
	cmd_buf[0] = 0x52;	// Tina add the Report Type
	for (i = 0; i < current_cmd_len; ++i) {
		cmd_buf[i+1] = fw_data_buf[current_cmd_idx + i];
		CRCRes += cmd_buf[i+1];	// CRC by add all bytes
	}

	// get sequence of data_cmd
	if (cmd_buf[4+1] == 0x01) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "send START_CMD\n");
	}

	// get sequence of data_cmd
	if (cmd_buf[4+1] == 0x02) {
		current_data_seq = cmd_buf[5+1] | (cmd_buf[6+1] << 8) ;
		if(isVerboseLog){
			__android_log_print(ANDROID_LOG_INFO, TAG, "send DATA_CMD, DATAP:0x%x, SQE:0x%X\n", cmd_buf[3+1], current_data_seq);
		}
		remain_data_len = remain_data_len - current_cmd_len;
		if (num1*649*100/fw_data_len >= num2 && num2 <= 99) {
			num2++;
			__android_log_print(ANDROID_LOG_INFO, TAG, "write data ... %d%c\n", num2, '%');
		}
		num1++;
		//__android_log_print(ANDROID_LOG_INFO, TAG, "update FW size:%d     current_cmd_len:%d      remain_data_len:%d\n", fw_data_len, current_cmd_len, remain_data_len);
	}

	// fill CRC field
	cmd_buf[current_cmd_len+1] = (CRCRes & 0x00FF);
	cmd_buf[current_cmd_len + 1+1] = (CRCRes & 0xFF00) >> 8;
//	printf("CRC:0x%X\n", CRCRes);

	// finish command, set a flag
	if (cmd_buf[4+1] == 0x03) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "send FINISH_CMD, set finish flag\n");
		isFinishCmd = 1;
	}

FLAG_RETRY_CMD:

	OTAStage = STAGE_RETRYING;
	// if retry 3 times still err, OTA fail
	send_count++;
	wait_count = 0;
	if (send_count > 3) {
		goto FLAG_OTA_FAIL;
	}
	if(send_count > 1){
		__android_log_print(ANDROID_LOG_INFO, TAG, "Retrying OTA CMD #%d....\n", send_count);
	}

	// send data command, +3 >> ignore HDR & SIZE field
//	hidWrite(cmd_buf, current_cmd_len + 2);	// include CRC
	ret = hidWrite(cmd_buf, current_cmd_len+1);	// exclude CRC
	if (ret < 0) {
		goto FLAG_OTA_FAIL;
	}

	

	// wait response right or fail retry
	while (1) {

		wait_count++;
		if (wait_count < OTA_WAIT_TIME) {

			if (OTAStage == STAGE_NEXT_CMD) {
				// response right, start to send next command
				goto FLAG_SNED_NEXT_CMD;
			} else if (OTAStage == STAGE_ERR_RETRY) {
				// response error, retry command
				goto FLAG_RETRY_CMD;
			} else if (OTAStage == STAGE_COMPLETE) {
				// response right, start to send finish command
				goto FLAG_OTA_SUCCESS;
			}

		} else {
			// after 15s no response, retry command
			goto FLAG_RETRY_CMD;
		}

		usleep(20000);	// 20ms
	}

FLAG_OTA_FAIL:

	flagRead = 0;
	__android_log_print(ANDROID_LOG_INFO, TAG, "OTA fail ~~======================================\n");
	__android_log_print(ANDROID_LOG_INFO, TAG, "OTA STOP @@======================================\n");
	quit_thread();
	quit_serv();
	
	//close(fd);
	return 0;

FLAG_OTA_SUCCESS:

	flagRead = 0;
	__android_log_print(ANDROID_LOG_INFO, TAG, "OTA end  !! ======================================\n");
	__android_log_print(ANDROID_LOG_INFO, TAG, "OTA STOP @@======================================\n");
	quit_thread();
	quit_serv();
	//close(fd);
	return 0;
}

// Don't execute (Permission Denied)
void* open_sock()
{
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    open_sock  \n");
	int lm = 0;
	
	csg = l2cap_listen(BDADDR_ANY, L2CAP_PSM_HIDP_CTRL , lm, 10);
		if (csg < 0) {
			__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    open_sock  listen L2CAP_PSM_HIDP_CTRL  ERR:%s\n",strerror(errno));
			perror("Can't listen on HID control channel");
			connection_status = -1;
		}

	isg = l2cap_listen(BDADDR_ANY, L2CAP_PSM_HIDP_INTR , lm, 10);
		if (isg < 0) {
			__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    open_sock  listen L2CAP_PSM_HIDP_INTR  ERR:%s\n",strerror(errno));
			perror("Can't listen on HID interrupt channel");
			close(csg);
			connection_status = -1;
		}

	ctrl = l2cap_accept(csg, NULL);
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    open_sock  l2cap_accept  L2CAP_PSM_HIDP_CTRL  ERR:%s\n",strerror(errno));
	
	intr = l2cap_accept(isg, NULL);	
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    open_sock  l2cap_accept  L2CAP_PSM_HIDP_INTR  ERR:%s\n",strerror(errno));
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    open_sock  csg:%d   isg:%d   ctrl:%d   intr:%d\n",csg, isg, ctrl, intr);

	connection_status = 1;
	
	pthread_exit(NULL);
	return ((void *)0);
}

void init_server()
{
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    init_server  \n");
	int hdev = 0;
	int  iret;
/*	uint8_t* dev_class;
	uint8_t* dev_class2;
	
	//Change device class
	dev_class = get_device_class(0);
	
	sprintf(default_class,"0x%02x%02x%02x\n", dev_class[2], dev_class[1], dev_class[0]);
	printf("Default device class: %s", default_class);
	
	set_device_class(hdev, "0x0005c0");

	dev_class2 = get_device_class(0);
	printf("Device Class changed to: 0x%02x%02x%02x\n", dev_class2[2], dev_class2[1], dev_class2[0]);
	class_st = 1; */
	

    /* Create thread */
    //iret = pthread_create( &thread, NULL, open_sock, NULL);
}



int connection_state()
{
	return connection_status;
}


int quit_serv()
{
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    quit_serv  \n");
	int hdev = 0;
    /*
	if (class_st == 1){

		set_device_class(hdev, default_class);
		printf("Device class changed to: %s\n", default_class);
	} */

	close(ctrl);
	close(intr);
	close(csg);
	close(isg);
	
	return 1;

}

void quit_thread()
{
	__android_log_print(ANDROID_LOG_INFO, TAG, " ======================================Tina    quit_thread  \n");
	int hdev = 0;
	int  iret;

    /*
	if (class_st == 1){

		set_device_class(hdev, default_class);
		printf("Device class changed to: %s\n", default_class);
	} */
	
	if (thread)
		iret = pthread_kill(thread, SIGTERM);
	printf("Thread finished\n");

}
