
#ifndef HIDSERV_H
#define HIDSERV_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
 #include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <netinet/in.h>
#include <sys/types.h> 
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
//#include <bluetooth/sdp.h>
#include <bluetooth/hidp.h>


#define L2CAP_PSM_HIDP_CTRL 0x11
#define L2CAP_PSM_HIDP_INTR 0x13


void	error(char *msg);
int		create_socket();
int		l2cap_connect(bdaddr_t *src, bdaddr_t *dst, unsigned short psm);
/*From Bluez Utils 3.2*/
static int 	l2cap_listen(const bdaddr_t *bdaddr, unsigned short psm, int lm, int backlog);
/*From Bluez Utils 3.2*/
static int 	l2cap_accept(int sk, bdaddr_t *bdaddr);
int 	reconnect(char *src, char *dst);
int		ota_update(char* param1, char* param2);

void	init_server();
int 	connection_state();
int 	quit_serv();
void	quit_thread();

//Private
static uint8_t* get_device_class(int hdev);
static void 	set_device_class(int hdev, char* class);
void* 		open_sock();

#endif
