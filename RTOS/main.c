/*
Tavoitettu pistemäärä viikko 4 tehtäviin: 2
Ajoitukset liikenne valoissa toimii ja debug viestit on vaihdettu omaan taskiin
*/

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <inttypes.h>

#define DEBUG

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
// Button pin configurations
#define BUTTON_0 DT_ALIAS(sw0)
#define BUTTON_1 DT_ALIAS(sw1)
#define BUTTON_2 DT_ALIAS(sw2)
#define BUTTON_3 DT_ALIAS(sw3)
#define BUTTON_4 DT_ALIAS(sw4)
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static const struct gpio_dt_spec button_1 = GPIO_DT_SPEC_GET_OR(BUTTON_1, gpios, {0});
static const struct gpio_dt_spec button_2 = GPIO_DT_SPEC_GET_OR(BUTTON_2, gpios, {0});
static const struct gpio_dt_spec button_3 = GPIO_DT_SPEC_GET_OR(BUTTON_3, gpios, {0});
static const struct gpio_dt_spec button_4 = GPIO_DT_SPEC_GET_OR(BUTTON_4, gpios, {0});
static struct gpio_callback button_0_data;
static struct gpio_callback button_1_data;
static struct gpio_callback button_2_data;
static struct gpio_callback button_3_data;
static struct gpio_callback button_4_data;

// Button handler initialization 
void button_0_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_1_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_2_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_3_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_4_handler(const struct device *, struct gpio_callback *, uint32_t);

// Led state configuration
/*
int led_state = 0;
int last_state = 0;
*/

// Define conditional values
K_MUTEX_DEFINE(red_mutex);
K_MUTEX_DEFINE(yellow_mutex);
K_MUTEX_DEFINE(green_mutex);
K_MUTEX_DEFINE(release_mutex);
K_CONDVAR_DEFINE(red_signal);
K_CONDVAR_DEFINE(yellow_signal);
K_CONDVAR_DEFINE(green_signal); 
K_CONDVAR_DEFINE(release_signal);

// Thread initialization
#define STACKSIZE 500
#define PRIORITY 5
#define DEBUG_PRIORITY 2

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);
K_FIFO_DEFINE(debug_fifo);

// FIFO dispatcher data type
struct data_t {
	void *fifo_reserved;
	char msg[30];
};



static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);

void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);

void debug_task(void *, void *, void *);

void debug_put(char[], ...);


K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(debug_thread,STACKSIZE,debug_task,NULL,NULL,NULL,DEBUG_PRIORITY,0,0);

// initialize init functions
int init_button(const struct gpio_dt_spec, struct gpio_callback *, gpio_callback_handler_t);
int init_led(void);
int init_uart(void);

// Main program
int main(void)
{
	timing_init();

	timing_start();
	timing_t start_time = timing_counter_get();

	int ret; 
	ret = init_uart();

	if (ret < 0) {
		printk("UART initialization failed!\n");
		return ret;
	}
	ret = init_led();
	if (ret < 0) 
		return ret;
	
	ret = init_button(button_0, &button_0_data, button_0_handler);
	if (ret < 0)
		return ret;
	ret = init_button(button_1, &button_1_data, button_1_handler);
	if (ret < 0)
		return ret;
	ret = init_button(button_2, &button_2_data, button_2_handler);
	if (ret < 0)
		return ret;
	ret = init_button(button_3, &button_3_data, button_3_handler);
	if (ret < 0)
		return ret;
	ret = init_button(button_4, &button_4_data, button_4_handler);
	if (ret < 0)
		return ret;
	timing_t stop_time = timing_counter_get();
	timing_stop();
	uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&start_time, &stop_time));
	printk("Initialization time: %lld ms\n", timing_ns/1000);
	return 0;
}

int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return -1;
	} 
	return 0;
}

// Button initialization
int init_button(const struct gpio_dt_spec spec, struct gpio_callback *callback, gpio_callback_handler_t handler) {

	int ret;
	if (!gpio_is_ready_dt(&spec)) {
		printk("Error: button is not ready\n");
		return -1;
	}
	
	ret = gpio_pin_configure_dt(&spec, GPIO_INPUT);
	if (ret < 0) {
		printk("Error: failed to configure pin\n");
		return ret;
	}
	
	ret = gpio_pin_interrupt_configure_dt(&spec, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return ret;
	}
	
	gpio_init_callback(callback, handler, BIT(spec.pin));
	gpio_add_callback(spec.port, callback);
	
	printk("Button setup OK\n");
	
	return 0;
}

// Initialize leds
int init_led(void) {

	// Led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printk("Error: Red led configure failed\n");		
		return ret;
	}
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printk("Error: Green led configure failed\n");		
		return ret;
	}
	ret = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printk("Error: Blue led configure failed\n");		
		return ret;
	} 
	// Set leds off
	gpio_pin_set_dt(&red,0);
	gpio_pin_set_dt(&green,0);
	gpio_pin_set_dt(&blue,0);

	printk("Leds initialized ok\n");
	
	return 0;
}


void debug_put(char msg[], ...){
	char debug_msg[30];
	memset(debug_msg,0,30);
	snprintf(debug_msg, 30, "%s", msg);
	struct data_t *debug_buf = k_malloc(sizeof(struct data_t));
	if (debug_buf == NULL) {
		return;
	}
	// Copy message
	snprintf(debug_buf->msg, 30, "%s", debug_msg);
	// Put debug data to FIFO buffer
	k_fifo_put(&debug_fifo,debug_buf);
	// Clear message buffer
	memset(debug_msg,0,30);
}

void debug_task(void *, void *, void *) {
	while(true){
		// Receive debug data from debug fifo
		struct data_t *rec_item = k_fifo_get(&debug_fifo, K_FOREVER);
		char debug_msg[30];
		memcpy(debug_msg,rec_item->msg,30);
		k_free(rec_item);
		printk("%s",debug_msg);
	}
}

static void uart_task(void *, void *, void *) {
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[30];
	memset(uart_msg,0,30);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {    
				struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 30, "%s", uart_msg);
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo,buf);
				// Clear UART receive buffer 
				uart_msg_cnt = 0;
				memset(uart_msg,0,30);

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,30);
			}
		}
		k_msleep(10);
	}
	return;
}

static void dispatcher_task(void *, void *, void *) {
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		char sequence[30];
		memcpy(sequence,rec_item->msg,30);
		k_free(rec_item);
		#ifdef DEBUG
			debug_put(sequence);
			debug_put("\n");
		#endif
		// Break down the dispacher sequence and send signals to proper tasks
		uint64_t time = 0;
		for(int i = 0;i < strlen(sequence);i++) {
			timing_start();
			timing_t start_time = timing_counter_get();
			if (sequence[i] == 'R') {
				#ifdef DEBUG
				debug_put("R\n");
				#endif
				k_condvar_broadcast(&red_signal);
				k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
			}
			if (sequence[i] == 'Y') {
				#ifdef DEBUG
				debug_put("YELLOW\n");
				#endif
				k_condvar_broadcast(&yellow_signal);
				k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
			}
			if (sequence[i] == 'G') {
				#ifdef DEBUG
				debug_put("GREEN\n");
				#endif
				k_condvar_broadcast(&green_signal);
				k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
			}
			timing_t stop_time = timing_counter_get();
			timing_stop();
			uint64_t timing_ns = timing_cycles_to_ns(timing_cycles_get(&start_time, &stop_time));
			char msg[30]; 
			debug_put("Thread stop time: ");
			snprintf(msg, 30, "%lld\n",timing_ns/1000);
			debug_put(msg);
			time += timing_ns;
		}
		char msg[30]; 
		debug_put("Full sequence: ");
		snprintf(msg, 30, "%lld\n",time/1000);
		debug_put(msg);
	}
}

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {}
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {}
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {}
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {}
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {}

// Task to handle red led
void red_led_task(void *, void *, void *) {
	
	debug_put("Red led thread started\n");
	while (true) {
		// Wait for signal
		k_condvar_wait(&red_signal, &red_mutex, K_FOREVER);
		#ifdef DEBUG
		debug_put("Red task running\n");
		#endif
		// Set led on 
		gpio_pin_set_dt(&red,1);
		// Sleep for 1 seconds
		k_sleep(K_SECONDS(1));
		// Set led off
		gpio_pin_set_dt(&red,0);
		// Send signal to dispacher
		k_condvar_broadcast(&release_signal);
	}
}
// Task to handle yellow led
void yellow_led_task(void *, void *, void *) {
	
	debug_put("Yellow led thread started\n");
	while (true) {
		// Wait for signal
		k_condvar_wait(&yellow_signal, &yellow_mutex, K_FOREVER);
		#ifdef DEBUG
		debug_put("Yellow task running\n");
		#endif
		// Set led on 
		gpio_pin_set_dt(&red,1);
		gpio_pin_set_dt(&green,1);
		// Sleep for 1 seconds
		k_sleep(K_SECONDS(1));
		// Set leds off
		gpio_pin_set_dt(&red,0);
		gpio_pin_set_dt(&green,0);
		// Send signal to dispacher
		k_condvar_broadcast(&release_signal);
	}
}
// Task to handle green led
void green_led_task(void *, void *, void *) {

	debug_put("Green led thread started\n");
	while (true) {
		// Wait for signal
		k_condvar_wait(&green_signal, &green_mutex, K_FOREVER);
		#ifdef DEBUG
		debug_put("Green task running\n");
		#endif
		// Set led on 
		gpio_pin_set_dt(&green,1);
		// Sleep for 1 seconds
		k_sleep(K_SECONDS(1));
		// Set led off
		gpio_pin_set_dt(&green,0);
		// Send signal to dispacher
		k_condvar_broadcast(&release_signal);
	}
}