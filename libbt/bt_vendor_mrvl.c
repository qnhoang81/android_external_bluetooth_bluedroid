#include <time.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <pthread.h>

#include "bt_vendor_lib.h"
#include <hardware/bluetooth.h>

#include "marvell_wireless.h"

#define LOG_TAG "bluedroid-mrvl"
#include <cutils/log.h>

#define info(fmt, ...)  ALOGI ("%s(L%d): " fmt,__FUNCTION__, __LINE__,  ## __VA_ARGS__)
#define debug(fmt, ...) ALOGD ("%s(L%d): " fmt,__FUNCTION__, __LINE__,  ## __VA_ARGS__)
#define warn(fmt, ...) ALOGW ("## WARNING : %s(L%d): " fmt "##",__FUNCTION__, __LINE__, ## __VA_ARGS__)
#define error(fmt, ...) ALOGE ("## ERROR : %s(L%d): " fmt "##",__FUNCTION__, __LINE__, ## __VA_ARGS__)
#define asrt(s) if(!(s)) ALOGE ("## %s assert %s failed at line:%d ##",__FUNCTION__, #s, __LINE__)
/*[NK] @Marvell - Driver FIX
   ioctl command to release the read thread before driver close */
#define MBTCHAR_IOCTL_RELEASE _IO  ('M',1)

static const bt_vendor_callbacks_t *vnd_cb = NULL;

static char bdaddr[17];

static char mchar_port[] = "/dev/mbtchar0";

static int mchar_fd = 0;

#define MAX_RETRY 2

int bt_vnd_mrvl_if_init(const bt_vendor_callbacks_t* p_cb, unsigned char *local_bdaddr)
{
    vnd_cb = p_cb;
    memcpy(bdaddr, local_bdaddr, sizeof(bdaddr));
    return 0;
}

int bt_vnd_mrvl_if_op(bt_vendor_opcode_t opcode, void *param)
{
    int ret = 0;
    int local_st = 0;

    debug("opcode = %d\n", opcode);
    switch (opcode) {
    case BT_VND_OP_POWER_CTRL:
        {
            int *state = (int *) param;
            if (*state == BT_VND_PWR_OFF) {
                debug("power off ***************************************\n");
                ret = bluetooth_disable();
                debug("bluetooth_disable, ret: 0x%x", ret);
                if (ret) {
                    /* Sometimes, driver has not detected the FW hung yet (driver need 9s to get this);  */
                    /* and so MarvellWirelessDaemon did not call bt_force_poweroff to recover the chip, */
                    /* which will lead to failure of bluetooth_disable. Then we need to do it here */
                    debug("Fail to disable BT, force power off");
                    if (!bt_fm_force_poweroff())ret = 0;
                }
            } else if (*state == BT_VND_PWR_ON) {
                debug("power on --------------------------------------\n");
                int retry = MAX_RETRY;
                while (retry-- > 0) {
                    ret = bluetooth_enable();
                    debug("bluetooth_enable, ret: 0x%x", ret);
                    if (!ret) break;
                    debug("Fail to enable BT the [%d] time, force power off", MAX_RETRY - retry);

                    /* bluetooth_enable failed, assume FW has hung */
                    if (bt_fm_force_poweroff())break;
                }
                if (ret) {
                    bluetooth_disable();
                }
            }
        }
        break;
    case BT_VND_OP_FW_CFG:
        // TODO: Perform any vendor specific initialization or configuration on the BT Controller
        // ret = xxx
        if (vnd_cb) {
            vnd_cb->fwcfg_cb(ret);
        }
        break;
    case BT_VND_OP_SCO_CFG:
        // TODO:  Perform any vendor specific SCO/PCM configuration on the BT Controller.
        // ret = xxx
        if (vnd_cb) {
            vnd_cb->scocfg_cb(ret);
        }
        break;
    case BT_VND_OP_USERIAL_OPEN:
        {
            int (*fd_array)[] = (int (*)[]) param;
            int idx;

            mchar_fd = open(mchar_port, O_RDWR);
            if (mchar_fd > 0) {
                debug("open %s successfully\n", mchar_port);
                for (idx=0; idx < CH_MAX; idx++) {
                    (*fd_array)[idx] = mchar_fd;
                        ret = 1;
                }
            } else {
                error("open %s failed error = %d\n", mchar_port, mchar_fd);
                ret = -1;
            }
        }
        break;
    case BT_VND_OP_USERIAL_CLOSE:
        /* mBtChar port is blocked on read. Release the port before we close it */
        ioctl(mchar_fd,MBTCHAR_IOCTL_RELEASE,&local_st);
        /* Give it sometime before we close the mbtchar */
        usleep(1000);
        if (mchar_fd) {
            if (close(mchar_fd) < 0) {
                error("");
                ret = -1;
            }
        }
        break;
    case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
        break;
    case BT_VND_OP_LPM_SET_MODE:
        // TODO: Enable or disable LPM mode on BT Controller.
        // ret = xx;
        if (vnd_cb) {
            vnd_cb->lpm_cb(ret);
        }
        break;
    case BT_VND_OP_LPM_WAKE_SET_STATE:
        break;
    default:
        ret = -1;
        break;
    }
    return ret;
}

void  bt_vnd_mrvl_if_cleanup(void)
{
    return;
}

const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    bt_vnd_mrvl_if_init,
    bt_vnd_mrvl_if_op,
    bt_vnd_mrvl_if_cleanup,
};



