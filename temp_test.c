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

/* helper macros */
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))

#define ASSERT(expr) { \
	if (!(expr)) { \
		(void) fprintf(stderr, "assertion failed (%s:%d)\n", __FILE__, __LINE__); \
		(void) abort(); \
	} \
}

/* RX is input, TX is output */
enum iodev { RX, TX };

/* common RX and TX streaming params */
struct stream_cfg {
	long long bw_hz; // Analog banwidth in Hz
	long long fs_hz; // Baseband sample rate in Hz
	long long lo_hz; // Local oscillator frequency in Hz
	const char* rfport; // Port name
};

/* static scratch mem for strings */
static char tmpstr[64];

/* IIO structs required for streaming */
static struct iio_context *ctx   = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *tx0_i = NULL;
static struct iio_buffer  *rxbuf = NULL;
static struct iio_buffer  *txbuf = NULL;

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

char* device0 = "ad7291\0";
int main(int argc, char **argv)
{
    //streaming devices
    struct iio_device *tx;
    struct iio_device *rx;

	// RX and TX sample counters
	size_t nrx = 0;
	size_t ntx = 0;

	// Listen to ctrl+c and ASSERT
	signal(SIGINT, handle_sig);

    // acquire iio context
    ctx = iio_create_local_context();
    printf("DEVICE COUNT: %u\n", iio_context_get_devices_count(ctx));

    rx = iio_context_find_device(ctx, device0);
    if (rx == NULL)
    {
        printf("COULDN'T FIND DEVICE\n");
        shutdown();
        return 0;
    }
    printf("CHOSE: %s\n", iio_device_get_name(rx));
    unsigned int channel_count = iio_device_get_channels_count(rx);
    printf("\n%u Channels Found\n", channel_count);

    int i;
    for (i = 0; i < channel_count; i++)
    {
        rx0_i = iio_device_get_channel(rx, i);
        if (rx0_i == NULL)
            printf("COULDN'T FIND CHANNEL %d\n", i);
        else
        {
            printf("%d: CHANNEL NAME: %s\n", i, iio_channel_get_name(rx0_i));
            printf("Channel id: %s\n",  iio_channel_get_id(rx0_i));
            printf("Is Input: %d\n", iio_channel_is_output(rx0_i));
            printf("Is Scan: %d\n", iio_channel_is_scan_element(rx0_i));
        }
    }

    //set to temp0
    //most of this loop is useless because of the note shown below
    int temp_index;
    for (i = 0; i < channel_count; i++)
    {
        rx0_i = iio_device_get_channel(rx, i);
        if (!strcmp(iio_channel_get_id(rx0_i), "temp0"))
        {
            temp_index = i;
            iio_channel_enable(rx0_i);
        }
        else
            iio_channel_disable(rx0_i);
    }
    rx0_i = iio_device_get_channel(rx, temp_index);

    /*NOTE: buffer cannot be created to read temp0 or any other channels for this device because
            all return false for 'iio_channel_is_scan_element'.  A channel has to be a scan element
            to be enabled, and a channel has to be enabled to use buffer
    */
    //rxbuf = iio_device_create_buffer(rx, 1024 * 1024, false);
    //if (rxbuf == NULL)
    //    printf("ERROR: BUFFER COULN'T BE CREATED\n");

    //index of attribute containing raw value
    const char* raw_filename = iio_channel_attr_get_filename(rx0_i, "raw");
    printf("\nFilename of raw data: %s\n", raw_filename);
    //it expects a char for some reason... it works tho
    char *rawval = (char*)malloc(sizeof(char)*20);    
    int num_samples = 25;
    for(i = 0; i < num_samples; i++){
        iio_channel_attr_read(rx0_i, "raw", rawval, sizeof(char)*20);
        printf("RAW TEMPERATURE: %s\n", rawval);
    }

    shutdown();
    return 0;
}