/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include <string.h>
#include "ili9341.h"
#include "lvgl.h"
#include "touch/touch.h"

LV_FONT_DECLARE(dseg70);
LV_FONT_DECLARE(dseg35);
LV_FONT_DECLARE(dseg30);
LV_FONT_DECLARE(dseg45);
LV_FONT_DECLARE(dseg60);

#define MAG_PIO           PIOA
#define MAG_PIO_ID        ID_PIOA
#define MAG_PIO_IDX       11
#define MAG_PIO_IDX_MASK  (1u << MAG_PIO_IDX)

QueueHandle_t xQueueMAG;

static void MAG_init(void);
static void task_mag(void *pvParameters);
static void mag_callback(void);

typedef struct 
{
	float velocity;
	float previus_velocity;
	float acceleration;
} bike_t;

/************************************************************************/
/* LCD / LVGL                                                           */
/************************************************************************/

#define LV_HOR_RES_MAX          (320)
#define LV_VER_RES_MAX          (240)

/*A static or global variable to store the buffers*/
static lv_disp_draw_buf_t disp_buf;

/*Static or global buffer(s). The second buffer is optional*/
static lv_color_t buf_1[LV_HOR_RES_MAX * LV_VER_RES_MAX];
static lv_disp_drv_t disp_drv;          /*A variable to hold the drivers. Must be static or global.*/
static lv_indev_drv_t indev_drv;

/* Global */

lv_obj_t * labelVelocidadeAtual;
static  lv_obj_t * labelClock;
static  lv_obj_t * labelTime;
static  lv_obj_t * labelDistancia;
static  lv_obj_t * labelPlay;
static  lv_obj_t * labelPause;
static  lv_obj_t * labelRefresh;
static  lv_obj_t * config;





lv_obj_t * btn2;
lv_obj_t * btn3;


volatile int flag_v_med = 0;
volatile int flag_v_now = 0;


/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/

#define TASK_LCD_STACK_SIZE                (1024*6/sizeof(portSTACK_TYPE))
#define TASK_LCD_STACK_PRIORITY            (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,  signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {	}
}

extern void vApplicationIdleHook(void) { }

extern void vApplicationTickHook(void) { }

extern void vApplicationMallocFailedHook(void) {
	configASSERT( ( volatile void * ) NULL );
}

/************************************************************************/
/* lvgl                                                                 */
/************************************************************************/

static void event_handler(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		LV_LOG_USER("Clicked");

	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void handler_v_med(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		printf("ENTROU NO HANDLER DESGRAÇADO");
		if (!flag_v_med){
			printf("COR VERDE");
			lv_color_t color = lv_color_make(0, 255, 0);
			lv_obj_set_style_text_color(btn2, color, LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(btn3, lv_color_white(), LV_STATE_DEFAULT);

			flag_v_med = 1;
		}else{
			flag_v_med = 0;
			lv_obj_set_style_text_color(btn2, lv_color_white(), LV_STATE_DEFAULT);

		}

	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void handler_v_now(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_CLICKED) {
		printf("ENTROU NO HANDLER DESGRAÇADO");
		if (!flag_v_now){
			printf("COR VERDE");
			lv_color_t color = lv_color_make(0, 255, 0);
			lv_obj_set_style_text_color(btn3, color, LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(btn2, lv_color_white(), LV_STATE_DEFAULT);
			flag_v_now = 1;
		}else{
			flag_v_now = 0;
			lv_obj_set_style_text_color(btn3, lv_color_white(), LV_STATE_DEFAULT);

		}

	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
		LV_LOG_USER("Toggled");
	}
}

static void handler_play(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	printf("ENTROU NO PLAY");

	if(code == LV_EVENT_CLICKED) {
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
	}
}

static void handler_pause(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	printf("ENTROU NO PAUSE");

	if(code == LV_EVENT_CLICKED) {
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
	}
}

static void handler_refresh(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	printf("ENTROU NO Refresh");

	if(code == LV_EVENT_CLICKED) {
	}
	else if(code == LV_EVENT_VALUE_CHANGED) {
	}
}

static void mag_callback(void)
{
	static uint32_t ul_previous_time;
	ul_previous_time = rtt_read_timer_value(RTT);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xQueueSendFromISR(xQueueMAG, &ul_previous_time, xHigherPriorityTaskWoken);
}

void lv_ex_btn_1(void) {
	
	static lv_style_t style;
	lv_style_init(&style);
	lv_style_set_bg_color(&style, lv_color_black());
	
	lv_obj_t * label;

	
	// lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
	// lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
	// lv_obj_align(btn1, LV_ALIGN_BOTTOM_LEFT, 0, -40);

	// label = lv_label_create(btn1);
	// lv_label_set_text(label, "Corsi");
	// lv_obj_center(label);

	/* Velocidade grande */
	labelVelocidadeAtual = lv_label_create(lv_scr_act());
	lv_obj_align(labelVelocidadeAtual, LV_ALIGN_TOP_LEFT, 20 , 0);
	lv_obj_set_style_text_font(labelVelocidadeAtual, &dseg70, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelVelocidadeAtual, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelVelocidadeAtual, "%02d", 23);

	/* Tempo da corrida */
	labelTime = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelTime, labelVelocidadeAtual, LV_ALIGN_BOTTOM_LEFT, -10, 40);
	lv_obj_set_style_text_font(labelTime, &dseg30, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelTime, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelTime, "%02d:%02d:%02d", 0, 0,0 );
	
	/* Distância percorrida */
	labelDistancia = lv_label_create(lv_scr_act());
	lv_obj_align_to(labelDistancia, labelVelocidadeAtual, LV_ALIGN_BOTTOM_LEFT, -10, 80);
	lv_obj_set_style_text_font(labelDistancia, &dseg30, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelDistancia, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelDistancia, "dist");


	/* km/ h  */
	lv_obj_t * km_h = lv_label_create(lv_scr_act());
	lv_obj_align_to(km_h, labelVelocidadeAtual, LV_ALIGN_OUT_RIGHT_BOTTOM, 0, 0);
	lv_obj_add_style(km_h, &style, 0);
	
	// Create an lv_color object using lv_color_make
    //lv_color_t color = lv_color_make(0, 255, 0);  // Creates a red color

	lv_obj_set_style_text_color(km_h, lv_color_white(), LV_STATE_DEFAULT);
	
	lv_label_set_text(km_h, " km/h") ;
	
	/* hora */
	labelClock = lv_label_create(lv_scr_act());
	lv_obj_align(labelClock, LV_ALIGN_TOP_RIGHT, 0 , 0);
	lv_obj_set_style_text_font(labelClock, &dseg30, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(labelClock, lv_color_white(), LV_STATE_DEFAULT);
	lv_label_set_text_fmt(labelClock, "%02d:%02d:%02d", 0, 0 );
		
	
	btn2 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn2, handler_v_med, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btn2, labelClock, LV_ALIGN_BOTTOM_MID, 0, 50);
	//lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_set_height(btn2, LV_SIZE_CONTENT);

	label = lv_label_create(btn2);
	lv_label_set_text(label, "Vmed");
	lv_obj_center(label);

	btn3 = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(btn3, handler_v_now, LV_EVENT_ALL, NULL);
	lv_obj_align_to(btn3, labelClock, LV_ALIGN_BOTTOM_MID, 0, 100);
	//lv_obj_add_flag(btn3, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_set_height(btn3, LV_SIZE_CONTENT);

	label = lv_label_create(btn3);
	lv_label_set_text(label, "Vnow");
	lv_obj_center(label);


	/* Player button */

	lv_obj_t * play = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(play, handler_play, LV_EVENT_ALL, NULL);
    lv_obj_align(play, LV_ALIGN_BOTTOM_LEFT, 20 , -50);
	lv_obj_add_style(play, &style, 0);

    labelPlay = lv_label_create(play);
    lv_label_set_text(labelPlay, "[  " LV_SYMBOL_PLAY " ]");
    lv_obj_center(labelPlay);
	
	/* Pause button */
	lv_obj_t * pause = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(pause, handler_pause, LV_EVENT_ALL, NULL);
    lv_obj_align_to(pause, play , LV_ALIGN_OUT_RIGHT_TOP, 20 , 0);
	lv_obj_add_style(pause, &style, 0);

    labelPause = lv_label_create(pause);
    lv_label_set_text(labelPause, "[ " LV_SYMBOL_PAUSE " ]");
    lv_obj_center(labelPause);

	/* Recomeçar button */
	lv_obj_t * refresh = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(refresh, handler_refresh, LV_EVENT_ALL, NULL);
    lv_obj_align_to(refresh, pause , LV_ALIGN_OUT_RIGHT_TOP, 20 , 0);
	lv_obj_add_style(refresh, &style, 0);

    labelRefresh = lv_label_create(refresh);
    lv_label_set_text(labelRefresh, "[ " LV_SYMBOL_REFRESH " ]");
    lv_obj_center(labelRefresh);

	/* Config */
	config = lv_btn_create(lv_scr_act());
	lv_obj_add_event_cb(config, handler_v_med, LV_EVENT_ALL, NULL);
	lv_obj_align_to(config, labelClock, LV_ALIGN_BOTTOM_MID, 0, 150);
	//lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
	lv_obj_set_height(config, LV_SIZE_CONTENT);

	label = lv_label_create(config);
	lv_label_set_text(label, "Config");
	lv_obj_center(label);
	

}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_lcd(void *pvParameters) {
	int px, py;

	lv_ex_btn_1();

	for (;;)  {
		lv_tick_inc(50);
		lv_task_handler();
		vTaskDelay(50);
	}
}

static void task_mag(void *pvParameters)
{
	MAG_init();
	rtt_init(RTT, 32);
	
	uint32_t ul_previous_time;
	bike_t bike;
	bike.velocity = 0;
	bike.previus_velocity = 0;
	
	float RADIUS = 0.20;
	float PI = 3.14159265358979f; 
	
	for (;;)
	{
		if (xQueueReceive(xQueueMAG, &ul_previous_time, (TickType_t) 100))
		{
			float period = (float) ul_previous_time / 1024;
			printf("Period: %.4f\nUL_PREV_TIME: %u\n", period, ul_previous_time);
			rtt_init(RTT, 32);
			float frequency = 1.0 / period;
			bike.previus_velocity = bike.velocity;
			bike.velocity = 2 * PI * frequency * RADIUS;
			
			bike.acceleration = bike.velocity - bike.previus_velocity / period;
			
			printf("Velocidade: %.4f\nAceleração: %.4f\n\n", bike.velocity, bike.acceleration);
		}
	}
}

/************************************************************************/
/* configs                                                              */
/************************************************************************/

static void configure_lcd(void) {
	/**LCD pin configure on SPI*/
	pio_configure_pin(LCD_SPI_MISO_PIO, LCD_SPI_MISO_FLAGS);  //
	pio_configure_pin(LCD_SPI_MOSI_PIO, LCD_SPI_MOSI_FLAGS);
	pio_configure_pin(LCD_SPI_SPCK_PIO, LCD_SPI_SPCK_FLAGS);
	pio_configure_pin(LCD_SPI_NPCS_PIO, LCD_SPI_NPCS_FLAGS);
	pio_configure_pin(LCD_SPI_RESET_PIO, LCD_SPI_RESET_FLAGS);
	pio_configure_pin(LCD_SPI_CDS_PIO, LCD_SPI_CDS_FLAGS);
	
	ili9341_init();
	ili9341_backlight_on();
}

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = USART_SERIAL_EXAMPLE_BAUDRATE,
		.charlength = USART_SERIAL_CHAR_LENGTH,
		.paritytype = USART_SERIAL_PARITY,
		.stopbits = USART_SERIAL_STOP_BIT,
	};

	/* Configure console UART. */
	stdio_serial_init(CONSOLE_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

/************************************************************************/
/* port lvgl                                                            */
/************************************************************************/

void my_flush_cb(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
	ili9341_set_top_left_limit(area->x1, area->y1);   ili9341_set_bottom_right_limit(area->x2, area->y2);
	ili9341_copy_pixels_to_screen(color_p,  (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
	
	/* IMPORTANT!!!
	* Inform the graphics library that you are ready with the flushing*/
	lv_disp_flush_ready(disp_drv);
}

void my_input_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
	int px, py, pressed;
	
	if (readPoint(&px, &py))
		data->state = LV_INDEV_STATE_PRESSED;
	else
		data->state = LV_INDEV_STATE_RELEASED; 
	
	data->point.x = px;
	data->point.y = py;
}

void configure_lvgl(void) {
	lv_init();
	lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, LV_HOR_RES_MAX * LV_VER_RES_MAX);
	
	lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
	disp_drv.draw_buf = &disp_buf;          /*Set an initialized buffer*/
	disp_drv.flush_cb = my_flush_cb;        /*Set a flush callback to draw to the display*/
	disp_drv.hor_res = LV_HOR_RES_MAX;      /*Set the horizontal resolution in pixels*/
	disp_drv.ver_res = LV_VER_RES_MAX;      /*Set the vertical resolution in pixels*/

	lv_disp_t * disp;
	disp = lv_disp_drv_register(&disp_drv); /*Register the driver and save the created display objects*/
	
	/* Init input on LVGL */
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_input_read;
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
}

static void MAG_init(void)
{
	pio_configure(MAG_PIO, PIO_INPUT, MAG_PIO_IDX_MASK, PIO_DEBOUNCE | PIO_DEFAULT);

	pio_handler_set(MAG_PIO,
					MAG_PIO_ID,
					MAG_PIO_IDX_MASK,
					PIO_IT_FALL_EDGE,
					&mag_callback);

	pio_enable_interrupt(MAG_PIO, MAG_PIO_IDX_MASK);
	pio_get_interrupt_status(MAG_PIO);
	
	NVIC_EnableIRQ(MAG_PIO_ID);
	NVIC_SetPriority(MAG_PIO_ID, 4); // Prioridade 4
}
/************************************************************************/
/* main                                                                 */
/************************************************************************/
int main(void) {
	/* board and sys init */
	board_init();
	sysclk_init();
	configure_console();

	/* LCd, touch and lvgl init*/
	configure_lcd();
	configure_touch();
	configure_lvgl();
	
	xQueueMAG = xQueueCreate(32, sizeof(uint32_t));
	if (xQueueMAG == NULL)
	printf("falha em criar a queue do bot");

	/* Create task to control oled */
	//if (xTaskCreate(task_lcd, "LCD", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
	//	printf("Failed to create lcd task\r\n");
	//}
	
	if (xTaskCreate(task_mag, "MAG", TASK_LCD_STACK_SIZE, NULL, TASK_LCD_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create mag task\r\n");
	}
	
	/* Start the scheduler. */
	vTaskStartScheduler();

	while(1){ }
}
