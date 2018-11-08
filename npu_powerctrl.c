// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
// Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LOG_TAG "NPU_POWER"
#include <utils/Log.h>
#include <cutils/properties.h>

#include <npu_powerctrl.h>

#define FNAME_SIZE 50
#define GPIO_BASE_PATH "/sys/class/gpio"
#define GPIO_EXPORT_PATH GPIO_BASE_PATH "/export"
#define GPIO_UNEXPORT_PATH GPIO_BASE_PATH "/unexport"

#define NPU_VDD_0V8_GPIO 	"4"  //GPIO0_PA4
#define NPU_VDD_LOG_GPIO 	"10" //GPIO0_PB2
#define NPU_VCC_1V8_GPIO 	"11" //GPIO0_PB3
#define NPU_VCC_DDR_GPIO 	NPU_VCC_1V8_GPIO
#define NPU_VDD_CPU_GPIO 	"54" //GPIO1_PC6
#define NPU_VCCIO_3V3_GPIO 	"55" //GPIO1_PC7
#define NPU_VDD_GPIO 		"56" //GPIO1_PD0

#define CPU_RESET_NPU_GPIO 	"32" //GPIO1_PA0
#define NPU_INT_CPU_GPIO 	"35" //GPIO1_A3
#define CPU_INT_NPU_GPIO 	"36" //GPIO1_A4

#define NPU_TEST_GPIO "96"

static char gpio_list[][4] = {NPU_VDD_0V8_GPIO, NPU_VDD_LOG_GPIO, NPU_VCC_1V8_GPIO, \
	NPU_VDD_CPU_GPIO, NPU_VCCIO_3V3_GPIO, NPU_VDD_GPIO, CPU_RESET_NPU_GPIO, \
	NPU_INT_CPU_GPIO, CPU_INT_NPU_GPIO};

static int sysfs_write(char *path, char *val) {
	char buf[80];
	int len;
	int fd = open(path, O_WRONLY);

	if (fd < 0) {
		strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error opening %s value=%s:%s\n", path, val, buf);
		return -1;
	}

	len = write(fd, val, sizeof(val));
	if (len < 0) {
		strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error writing to %s value=%s: %s\n", path, val, buf);
		return -1;
	}

	close(fd);
	return 0;
}

static void sysfs_read(char *path, char *val)
{
	char buf[80];
	int len;
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error opening %s value=%s:%s\n", path, val, buf);
		return;
	}

	len = read(fd, val, 1);
	if (len < 0) {
		strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error reading to %s value=%s: %s\n", path, val, buf);
	}

	close(fd);
}

static int clk_enable(int enable) {
	if (enable) {
		//set clk
		sysfs_write("/sys/kernel/debug/clk/clk_wifi_pmu/clk_rate", "24000000");
		//enable clk
		sysfs_write("/sys/kernel/debug/clk/clk_wifi_pmu/clk_enable_count", "1");
		/*to do, gpio0_a2, set iomux to 26M clk out*/
	} else
		sysfs_write("/sys/kernel/debug/clk/clk_wifi_pmu/clk_enable_count", "0");
	return 0;
}

static int request_gpio(char *gpio_num) {
	int ret;
	ret = sysfs_write(GPIO_EXPORT_PATH, gpio_num);
	return ret;
}

static void free_gpio(char *gpio_num) {
	sysfs_write(GPIO_UNEXPORT_PATH, gpio_num);
}

static void set_gpio_dir(char *gpio_num, char *dir) {
	char gpio_dir_name[FNAME_SIZE];

	snprintf(gpio_dir_name, sizeof(gpio_dir_name), "%s/gpio%s/direction",
			GPIO_BASE_PATH, gpio_num);
	sysfs_write(gpio_dir_name, dir);
}

static int get_gpio(char *gpio_number) {
	char gpio_name[FNAME_SIZE];
	char value;

	snprintf(gpio_name, sizeof(gpio_name), "%s/gpio%s/value",
			GPIO_BASE_PATH, gpio_number);

	sysfs_read(gpio_name, &value);
	if (value == 48 || value == 49)
		value -= 48;
	else
		value = -1;
	return (int)value;
}

static int set_gpio(char *gpio_number, char *val) {
	char gpio_val_name[FNAME_SIZE];

	snprintf(gpio_val_name, sizeof(gpio_val_name), "%s/gpio%s/value",
			GPIO_BASE_PATH, gpio_number);
	sysfs_write(gpio_val_name, val);
	return 0;
}

void npu_power_gpio_init(void) {
	int ret, index = 0, gpio_cnt = sizeof(gpio_list)/sizeof(int);

	while (index != gpio_cnt) {
		ALOGD("init gpio: %s\n", gpio_list[index]);
		ret = request_gpio(gpio_list[index]);
		if (ret) {
			ALOGD("init gpio: %s failed!!!\n", gpio_list[index]);
			printf("init gpio: %s failed!!!\n", gpio_list[index]);
			return;
		}
		set_gpio_dir(gpio_list[index], "out");
		index ++;
	}
	set_gpio_dir(NPU_INT_CPU_GPIO, "in");
}

void npu_power_gpio_exit(void) {
	int index = 0, gpio_cnt = sizeof(gpio_list)/sizeof(int);

	while (index != gpio_cnt) {
		ALOGD("init gpio: %s\n", gpio_list[index]);
		free_gpio(gpio_list[index]);
		index ++;
	}
}

void npu_reset(void) {
	sysfs_write("/sys/power/wake_lock", "npu_lock");
	/*power off*/
	set_gpio(NPU_VDD_LOG_GPIO, "0");
	/* wait for usb disconnect */
	usleep(2000);
	set_gpio(NPU_VDD_GPIO, "0");
	set_gpio(NPU_VCCIO_3V3_GPIO, "0");
	set_gpio(NPU_VDD_CPU_GPIO, "0");
	set_gpio(NPU_VCC_1V8_GPIO, "0");
	set_gpio(NPU_VDD_0V8_GPIO, "0");
	set_gpio(CPU_INT_NPU_GPIO, "0");
	set_gpio(CPU_RESET_NPU_GPIO, "0");
	usleep(2000);

	/*power en*/
	set_gpio(NPU_VDD_0V8_GPIO, "1");
	usleep(2000);
	set_gpio(NPU_VDD_LOG_GPIO, "1");
	usleep(2000);
	set_gpio(NPU_VCC_1V8_GPIO, "1");
	usleep(2000);
	clk_enable(1);
	set_gpio(NPU_VDD_CPU_GPIO, "1");
	usleep(2000);
	set_gpio(NPU_VCCIO_3V3_GPIO, "1");
	usleep(2000);
	set_gpio(NPU_VDD_GPIO, "1");
	usleep(2000);
	set_gpio(CPU_INT_NPU_GPIO, "1");

	usleep(25000);
	set_gpio(CPU_RESET_NPU_GPIO, "1");
}

void npu_poweroff(void) {
	/*power off*/
	set_gpio(NPU_VDD_LOG_GPIO, "0");
	/* wait for usb disconnect */
	usleep(2000);
	set_gpio(NPU_VDD_GPIO, "0");
	set_gpio(NPU_VCCIO_3V3_GPIO, "0");
	set_gpio(NPU_VDD_CPU_GPIO, "0");
	set_gpio(NPU_VCC_1V8_GPIO, "0");
	set_gpio(NPU_VDD_0V8_GPIO, "0");
	set_gpio(CPU_INT_NPU_GPIO, "0");
	set_gpio(CPU_RESET_NPU_GPIO, "0");
	clk_enable(0);
}

int npu_suspend(void) {
	int retry=100;

	if (get_gpio(NPU_INT_CPU_GPIO))
		return 0;

	set_gpio(CPU_INT_NPU_GPIO, "0");
	usleep(100000);
	set_gpio(CPU_INT_NPU_GPIO, "1");

	/*wait for npu enter sleep*/
	while (retry--) {
		if (get_gpio(NPU_INT_CPU_GPIO)) {
			set_gpio(NPU_VDD_CPU_GPIO, "0");
			set_gpio(NPU_VDD_GPIO, "0");
			clk_enable(0);

			sysfs_write("/sys/power/wake_unlock", "npu_lock");
			break;
		}
		usleep(10000);
	}

	if (!retry) {
		ALOGE("npu suspend timeout in one second\n");
		return -1;
	}
	return 0;
}

int npu_resume(void) {
	int retry=100;

	if (!get_gpio(NPU_INT_CPU_GPIO))
		return 0;

	clk_enable(1);
	set_gpio(NPU_VDD_CPU_GPIO, "1");
	set_gpio(NPU_VDD_GPIO, "1");

	usleep(10000);

	set_gpio(CPU_INT_NPU_GPIO, "0");
	usleep(100000);
	set_gpio(CPU_INT_NPU_GPIO, "1");
	/*wait for npu wakup*/
	while (retry--) {
		if (!get_gpio(NPU_INT_CPU_GPIO)) {
			sysfs_write("/sys/power/wake_lock", "npu_lock");
			break;
		}
		usleep(10000);
	}
	if (!retry) {
		ALOGE("npu resume timeout in one second\n");
		return -1;
	}

	return 0;
}

int npu_power_ctrl_test(void) {
	int val;

	request_gpio(NPU_TEST_GPIO);
	set_gpio_dir(NPU_TEST_GPIO, "out");
	while(1) {
		set_gpio(NPU_TEST_GPIO, "1");
		val = get_gpio(NPU_TEST_GPIO);
		ALOGE("set high：writing to %d\n", val);
		usleep(1000000);

		set_gpio(NPU_TEST_GPIO, "0");
		val = get_gpio(NPU_TEST_GPIO);
		ALOGE("set low: writing to %d\n", val);
		usleep(1000000);
	}
	return 0;
}
