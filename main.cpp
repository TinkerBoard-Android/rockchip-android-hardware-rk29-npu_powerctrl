// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
// Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd

#define LOG_TAG "NPU_POWER"
#include <utils/Log.h>
#include <cutils/properties.h>
#include <npu_powerctrl.h>

static void usage(void)
{
	ALOGD("Usage:npu_powerctrl [-s] [-r] [-o]");
	ALOGD("-s	npu enter sleep");
	ALOGD("-r	wakup npu");
	ALOGD("-o	power up or reset npu");
}

int main(int argc, char* argv[])
{
	int ch, ret = -1;

	while ((ch = getopt(argc, argv, "s::r::o::")) != -1) {
		switch (ch) {
			case 's':
				ret = npu_suspend();
				ALOGD("suspend %d\n", ret);
				break;
			case 'r':
				ret = npu_resume();
				ALOGD("resume %d\n", ret);
				break;
			case 'o':
				ret = 0;
				npu_reset();
				ALOGD("powerup\n");
				break;
			default:
				usage();
				break;
		}
		return ret;
	}
	usage();
  
	return ret;
}
