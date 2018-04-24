#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

#ifdef __APPLE__
#include <iio/iio.h>
#else
#include <iio.h>
#endif

#define ASSERT(expr) { \
	if (!(expr)) { \
		(void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
		(void) abort(); \
	} \
}

/* RX is input, TX is output */
enum iodev { RX, TX };

/* IIO structs required for streaming */
static struct iio_context *ctx   = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *tx0_i = NULL;
static struct iio_device *tx;
static struct iio_device *rx;


static bool stop;

/* cleanup and exit */
static void shutdown()
{
	printf("* Destroying buffers\n");
	if (rxbuf) { iio_buffer_destroy(rxbuf); }
	if (txbuf) { iio_buffer_destroy(txbuf); }

	printf("* Disabling streaming channels\n");
	if (rx0_i) { iio_channel_disable(rx0_i); }
	if (tx0_i) { iio_channel_disable(tx0_i); }

	printf("* Destroying context\n");
	if (ctx) { iio_context_destroy(ctx); }
	exit(0);
}

static void handle_sig(int sig)
{
	printf("Waiting for process to finish...\n");
	stop = true;
}

void getAttributeInfo(struct iio_channel *chn)
{
    unsigned int attrcount = iio_channel_get_attrs_count(chn);
    char *value = (char*)malloc(sizeof(char)*100);
    int i;
    for (i = 0; i < attrcount; i++)
    {
        const char *attrname = iio_channel_get_attr(chn, i);
        printf("\t\tATTR name: %s\n", attrname);
        iio_channel_attr_read(chn, attrname, value, sizeof(char)*200);
        printf("\t\t\tSample Read Data: %s\n", value);
    }
}

void getDeviceInfo(struct iio_device *device) {
    unsigned int channelCount = iio_device_get_channels_count(device);
    printf("\n%u Channels Found\n", channelCount);
    int i;
    for (i = 0; i < channelCount; i++)
    {
        rx0_i = iio_device_get_channel(device, i);
        if (rx0_i == NULL)
        {
            printf("COULDN'T FIND CHANNEL %d\n", i);
            continue;
        }
        //channel id represents 'name' file in /sys/bus/iio/devices
        printf("\tChannel id: %s\n",  iio_channel_get_id(rx0_i));
        printf("\t\tIs Output: %d\n", iio_channel_is_output(rx0_i));
        printf("\t\tIs Scan (able to be buffer'd) %d\n", iio_channel_is_scan_element(rx0_i));
        getAttributeInfo(rx0_i);
    }
}

int main(int argc, char **argv)
{
	// Listen to ctrl+c and ASSERT
	signal(SIGINT, handle_sig);

    // acquire iio context
    ctx = iio_create_local_context();
    unsigned int deviceCount = iio_context_get_devices_count(ctx);
    printf("DEVICE COUNT: %u\n", deviceCount);
    unsigned int i;
    for (i = 0; i < deviceCount; i++)
    {
        rx = iio_context_get_device(ctx, i);
        if (rx == NULL)
        {
            printf("COULDN'T FIND DEVICE\n");
            continue;
        }
        printf("\nDEVICE %d: %s\n", i, iio_device_get_name(rx));
        getDeviceInfo(rx);
    }

    shutdown();
    return 0;
}