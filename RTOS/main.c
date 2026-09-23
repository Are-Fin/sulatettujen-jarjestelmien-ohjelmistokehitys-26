#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/uart.h>
#include <inttypes.h>

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

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);

// FIFO dispatcher data type
struct data_t {
	void *fifo_reserved;
	char msg[20];
};

static void uart_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);

void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);


K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

// initialize init functions
int init_button(const struct gpio_dt_spec, struct gpio_callback *, gpio_callback_handler_t);
int init_led(void);
int init_uart(void);

// Main program
int main(void)
{
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

 
static void uart_task(void *, void *, void *) {
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			// printk("Received: %c\n",rc);
			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
				printk("UART msg: %s\n", uart_msg);
                
				struct data_t *buf = k_malloc(sizeof(struct data_t));
				if (buf == NULL) {
					return;
				}
				// Copy UART message to dispatcher data
				snprintf(buf->msg, 20, "%s", uart_msg);
				// Put dispatcher data to FIFO buffer
				k_fifo_put(&dispatcher_fifo,buf);
				// Clear UART receive buffer 
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);

				// Clear UART message buffer
				uart_msg_cnt = 0;
				memset(uart_msg,0,20);
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
		char sequence[20];
		memcpy(sequence,rec_item->msg,20);
		k_free(rec_item);

		printk("Dispatcher: %s\n", sequence);
		// Break down the dispacher sequence and send signals to proper tasks
		for(int i = 0;i < strlen(sequence);i++) {
			if (sequence[i] == 'R') {
				printk("RED\n");
				k_condvar_broadcast(&red_signal);
				k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
			}
			if (sequence[i] == 'Y') {
				printk("YELLOW\n");
				k_condvar_broadcast(&yellow_signal);
				k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
			}
			if (sequence[i] == 'G') {
				printk("GREEN\n");
				k_condvar_broadcast(&green_signal);
				k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
			}
			
		}
        // You need to:
        // Parse color and time from the fifo data
        // Example
        //    char color = sequence[0];
        //    int time = atoi(sequence+2);
		//    printk("Data: %c %d\n", color, time);
        // Send the parsed color information to tasks using fifo
        // Use release signal to control sequence or k_yield
	}
}

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	// Pressing button 0 pauses the led state
	// Pressing button 0 again resumes from the last active state
	// State cannot be paused while state 4 is active
	/*
	if (led_state < 3) {
		last_state = led_state;
		led_state = 3;
		printk("State paused on %d and changed to %d\n", last_state, led_state);
	} else if (led_state == 4) {
		printk("Failed to pause state\n");
	} else {
		led_state = last_state;
		printk("State resumed on %d\n", last_state);
	}
	*/
}

void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	/*
	if (led_state == 3) {
		gpio_pin_toggle_dt(&red);
		printk("Toggle red led\n");
	} else {
		printk("Failed to toggle red led\n");
	}
	*/
}

void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	/*
	if (led_state == 3) {
		gpio_pin_toggle_dt(&green);
		printk("Toggle green led\n");
	} else {
		printk("Failed to toggle green led\n");
	}
	*/
}

void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	/*
	if (led_state == 3) {
		gpio_pin_toggle_dt(&blue);
		printk("Toggle blue led\n");
	} else {
		printk("Failed to toggle blue led\n");
	}
	*/
}

void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	/*
	// Changes state to 4 to start the flicker sequence
	if (led_state != 4) {
		led_state = 4;
		printk("State changed to %d\n", led_state);
	} else {
		led_state = 0;
		printk("State reset to %d\n", led_state);
	}
	*/
}
// Task to handle red led
void red_led_task(void *, void *, void *) {
	
	printk("Red led thread started\n");
	while (true) {
		// Wait for signal
		k_condvar_wait(&red_signal, &red_mutex, K_FOREVER);
		// Set led on 
		gpio_pin_set_dt(&red,1);
		printk("Red on\n");
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
	
	printk("Yellow led thread started\n");
	while (true) {
		// Wait for signal
		k_condvar_wait(&yellow_signal, &yellow_mutex, K_FOREVER);
		// Set led on 
		gpio_pin_set_dt(&red,1);
		gpio_pin_set_dt(&green,1);
		printk("Yellow on\n");
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

	printk("Green led thread started\n");
	while (true) {
		// Wait for signal
		k_condvar_wait(&green_signal, &green_mutex, K_FOREVER);
		// Set led on 
		gpio_pin_set_dt(&green,1);
		printk("Green on\n");
		// Sleep for 1 seconds
		k_sleep(K_SECONDS(1));
		// Set led off
		gpio_pin_set_dt(&green,0);
		// Send signal to dispacher
		k_condvar_broadcast(&release_signal);
	}
}