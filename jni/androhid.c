#include "androhid.h"
#include <jni.h>
#include <string.h>

#include "hidserv.h"
#include "sdp.h"

/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    setupClient
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_example_pmxota_OtaLoadJNI_setupClientSocket(JNIEnv *env, jobject obj) {
	if (hci_get_route(NULL) < 0) {
		return -1;
	}
	// Open connection to Service Descovery Protokoll Server
	//if (sdp_open() < 0)
	//	return -2;
	// Add the HID Service
	//sdp_add_keyboard();
	//return init_sdp_server(0,0);
	
	// Initialize the bluetooth L2CAP connection
	init_server();
	return 0;
}

/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    connectToHost
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_example_pmxota_OtaLoadJNI_connectToHost(JNIEnv *env, jobject obj, jstring BtAddress) {
	const char *buffer = (*env)->GetStringUTFChars(env, BtAddress, 0);
	int returnValue = 0;
	//Getting local bluetooth address
	char localBtAddress[64] = { 0 };
	bdaddr_t loc_addr = *BDADDR_ANY;
	ba2str(&loc_addr, localBtAddress);

	//Connecting to HID host
	returnValue = reconnect(localBtAddress, buffer);

	(*env)->ReleaseStringUTFChars(env, BtAddress, buffer);
	return returnValue;
}

/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    deconnectClient
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_example_pmxota_OtaLoadJNI_deconnectClient(JNIEnv *env, jobject obj) {
	// Remove the HID service from sdp server
	//sdp_remove();
	// Kill hid interrupt and control channels
	quit_thread();
	// Finish bt sockets
	quit_serv();
	return 0;
}
  

/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    startOTAUpdate
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_example_pmxota_OtaLoadJNI_startOTAUpdate(JNIEnv *env, jobject obj, jstring fwFilePath, jstring isVerboseLogS) {
 	return ota_update((char*)(*env)->GetStringUTFChars(env, fwFilePath, 0),
						(char*)(*env)->GetStringUTFChars(env, isVerboseLogS, 0));
}

/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    btDeviceScan
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_example_pmxota_OtaLoadJNI_btDeviceScan(JNIEnv *env, jobject obj) {
	inquiry_info *ii = NULL;
	int max_rsp = 255;
	// length of device scan * 1.25 sec
	int len = 8;
	int num_rsp;
	int dev_id, flags, i;
	char addr[19] = {0};
	char name[248] = {0};
	int bHasError = 0;
	jstring errMsg;    

	jobjectArray btDevices; 

	dev_id = hci_get_route(NULL);
	if (dev_id < 0) {
		bHasError = 1;
		errMsg = (*env)->NewStringUTF(env, "Error obtaining device ID");
	}

	flags = IREQ_CACHE_FLUSH;
	num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	if (num_rsp < 0)
		perror("hci_inquiry failed");

	btDevices = (*env)->NewObjectArray(env, num_rsp, (*env)->FindClass(env, "java/lang/String"), (*env)->NewStringUTF(env, ""));

	for (i=0; i<num_rsp; i++) {
		ba2str(&(ii+i)->bdaddr, addr);

		(*env)->SetObjectArrayElement(env, btDevices, i, (*env)->NewStringUTF(env, addr));
	}
	return btDevices;
}

/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    btDeviceGetName
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_example_pmxota_OtaLoadJNI_btDeviceGetName(JNIEnv *env, jobject obj, jstring btAddressString) {
	const char *buffer = (*env)->GetStringUTFChars(env, btAddressString, 0);
	bdaddr_t   btAddress;
	char name[248] = {0};
	int dev_id, sock;

	str2ba(buffer, &btAddress);

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev(dev_id);

	// if sock or dev_id returned with error
	if (dev_id<0 || sock<0) {
		close(sock);
		return (*env)->NewStringUTF(env, "[unknown]");
	}

	// Get the name of the remote bt Device
	memset(name, 0, sizeof(name));
	if (hci_read_remote_name(sock, &btAddress, sizeof(name), name, 0) < 0)
		strcpy(name, "[unknown]");

	close(sock);
	return (*env)->NewStringUTF(env, name);
}


/*
 * Class:     com_example_pmxota_OtaLoadJNI
 * Method:    isEnabled
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_example_pmxota_OtaLoadJNI_isEnabled(JNIEnv *env, jobject obj) {
	if(hci_get_route(NULL) < 0){
		return JNI_FALSE;
	}
	return JNI_TRUE;
}
