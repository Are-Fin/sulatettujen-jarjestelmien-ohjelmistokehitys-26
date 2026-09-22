/*
Tavoitettu pistemäärä viikko 2 tehtäviin: 3
Tilakone toimii, nappi 1 pysäyttää tilakoneen, 
napit 2-4 vaihtavat ledejä kun tilakone on pysäytetty ja 
nappi 5 näyttää sekvenssinä vilkkuvaa keltaista valoa.
*/
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
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

// Button interrupt initialization 
void button_0_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_1_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_2_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_3_handler(const struct device *, struct gpio_callback *, uint32_t);
void button_4_handler(const struct device *, struct gpio_callback *, uint32_t);

// Led state configuration
int led_state = 0;
int last_state = 0;
// Red led thread initialization
#define STACKSIZE 500
#define PRIORITY 5
void red_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
void yellow_flicker_task(void *, void *, void*);
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_flicker_thread,STACKSIZE,yellow_flicker_task,NULL,NULL,NULL,PRIORITY,0,0);



// Main program
int main(void)
{
	int ret; 
	ret = init_led();
	if (ret < 0) {
		return ret;
	}
	
	ret = init_button();
	if (ret < 0) {
		return ret;
	}
	return 0;
}


// Button initialization
int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_0)) {
		printk("Error: button 0 is not ready\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&button_1)) {
		printk("Error: button 1 is not ready\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&button_2)) {
		printk("Error: button 2 is not ready\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&button_3)) {
		printk("Error: button 3 is not ready\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&button_4)) {
		printk("Error: button 4 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}
	ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}
	ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}
	ret = gpio_pin_configure_dt(&button_3, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}
	ret = gpio_pin_configure_dt(&button_4, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&button_1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&button_2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&button_3, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&button_4, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_init_callback(&button_1_data, button_1_handler, BIT(button_1.pin));
	gpio_init_callback(&button_2_data, button_2_handler, BIT(button_2.pin));
	gpio_init_callback(&button_3_data, button_3_handler, BIT(button_3.pin));
	gpio_init_callback(&button_4_data, button_4_handler, BIT(button_4.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	gpio_add_callback(button_1.port, &button_1_data);
	gpio_add_callback(button_2.port, &button_2_data);
	gpio_add_callback(button_3.port, &button_3_data);
	gpio_add_callback(button_4.port, &button_4_data);
	printk("Set up buttons 0 to 4 ok \n");
	
	return 0;
}

// Initialize leds
int init_led() {

	// Led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Red led configure failed\n");		
		return ret;
	}
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Green led configure failed\n");		
		return ret;
	}
	ret = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
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
// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	// Pressing button 0 pauses the led state
	// Pressing button 0 again resumes from the last active state
	// State cannot be paused while state 4 is active
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
}

void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	if (led_state == 3) {
		gpio_pin_toggle_dt(&red);
		printk("Toggle red led\n");
	} else {
		printk("Failed to toggle red led\n");
	}
}

void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	if (led_state == 3) {
		gpio_pin_toggle_dt(&green);
		printk("Toggle green led\n");
	} else {
		printk("Failed to toggle green led\n");
	}
}

void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	if (led_state == 3) {
		gpio_pin_toggle_dt(&blue);
		printk("Toggle blue led\n");
	} else {
		printk("Failed to toggle blue led\n");
	}
}

void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	// Changes state to 4 to start the flicker sequence
	if (led_state != 4) {
		led_state = 4;
		printk("State changed to %d\n", led_state);
	} else {
		led_state = 0;
		printk("State reset to %d\n", led_state);
	}
}
// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	printk("Red led thread started\n");
	while (true) {
		if (led_state == 0) {
			// Set led on 
			gpio_pin_set_dt(&red,1);
			gpio_pin_set_dt(&green,0);
			gpio_pin_set_dt(&blue,0);
			printk("Red on\n");
			// Sleep for 1 seconds
			k_sleep(K_SECONDS(1));
			// Change led state if state isn't paused
			if (led_state < 3) {
				led_state = 1;
				printk("Red off\n");
			}
		}
		k_msleep(50);
	}
}
// Task to handle yellow led
void yellow_led_task(void *, void *, void*) {
	
	printk("Yellow led thread started\n");
	while (true) {
		if (led_state == 1) {
			// Set led on 
			gpio_pin_set_dt(&red,1);
			gpio_pin_set_dt(&green,1);
			gpio_pin_set_dt(&blue,0);
			printk("Yellow on\n");
			// Sleep for 1 seconds
			k_sleep(K_SECONDS(1));
			// Change led state if state isn't paused
			if (led_state < 3) {
				led_state = 2;
				printk("Yellow off\n");
			}
		}
		k_msleep(50);
	}
}
// Task to handle green led
void green_led_task(void *, void *, void*) {

	printk("Green led thread started\n");
	while (true) {
		if (led_state == 2) {
			// Set led on 
			gpio_pin_set_dt(&red,0);
			gpio_pin_set_dt(&green,1);
			gpio_pin_set_dt(&blue,0);
			printk("Green on\n");
			// Sleep for 1 seconds
			k_sleep(K_SECONDS(1));
			// Change led state if state isn't paused
			if (led_state < 3) {
				led_state = 0;
				printk("Green off\n");
			}
		}
		k_msleep(50);
	}
}
void yellow_flicker_task(void *, void *, void*) {
	printk("Yellow flicker task started\n");
	while(true) {
		if (led_state == 4) {
			// Set leds on
			gpio_pin_set_dt(&red,1);
			gpio_pin_set_dt(&green,1);
			gpio_pin_set_dt(&blue,0);
			// Sleep for 0.25 seconds 
			k_sleep(K_MSEC(250));
			// Set leds off
			gpio_pin_set_dt(&red,0);
			gpio_pin_set_dt(&green,0);
			gpio_pin_set_dt(&blue,0);
			// Sleep for 0.25 seconds 
			k_sleep(K_MSEC(250));
		}
		k_msleep(50);
	}
}