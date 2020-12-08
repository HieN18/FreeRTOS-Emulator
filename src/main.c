#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define CIRCLE_X SCREEN_WIDTH / 3
#define CIRCLE_Y SCREEN_HEIGHT / 2
#define CIRCLE_RADIUS 30

#define TRIANGLE_SIDE_LENGTH 60
#define CORNER_1_X SCREEN_WIDTH / 2
#define CORNER_1_Y SCREEN_HEIGHT / 2 - TRIANGLE_SIDE_LENGTH *sqrt(3) / 4
#define CORNER_2_X SCREEN_WIDTH / 2 - TRIANGLE_SIDE_LENGTH / 2
#define CORNER_2_Y SCREEN_HEIGHT / 2 + TRIANGLE_SIDE_LENGTH *sqrt(3) / 4
#define CORNER_3_X SCREEN_WIDTH / 2 + TRIANGLE_SIDE_LENGTH / 2
#define CORNER_3_Y SCREEN_HEIGHT / 2 + TRIANGLE_SIDE_LENGTH *sqrt(3) / 4

#define SQUARE_SIDE_LENGTH 50

static TaskHandle_t DemoTask = NULL;

static coord_t points[3] = { { CORNER_1_X, CORNER_1_Y },
			     { CORNER_2_X, CORNER_2_Y },
			     { CORNER_3_X, CORNER_3_Y } };

typedef struct circle {
	signed short x; /**< X pixel coord of ball on screen */
	signed short y; /**< Y pixel coord of ball on screen */
	unsigned int colour; /**< Hex RGB colour of the ball */
	signed short radius; /**< Radius of the ball in pixels */
} circle_t;
circle_t *createCircle(signed short initial_x, signed short initial_y,
		       unsigned int colour, signed short radius)
{
	circle_t *ret = calloc(1, sizeof(circle_t));

	if (!ret) {
		fprintf(stderr, "Creating circle failed\n");
		exit(EXIT_FAILURE);
	}

	ret->x = initial_x;
	ret->y = initial_y;
	ret->colour = colour;
	ret->radius = radius;

	return ret;
}

typedef struct square {
	signed short x; /**< X pixel coord of ball on screen */
	signed short y; /**< Y pixel coord of ball on screen */
	signed short w; /**< W pixel coord of ball on screen */
	signed short h; /**< H pixel coord of ball on screen */
	unsigned int colour; /**< Hex RGB colour of the ball */
} square_t;

square_t *createSquare(signed short initial_x, signed short initial_y,
		       signed short initial_w, signed short initial_h,
		       unsigned int colour)

{
	square_t *ret = calloc(1, sizeof(square_t));

	if (!ret) {
		fprintf(stderr, "Creating square failed\n");
		exit(EXIT_FAILURE);
	}

	ret->x = initial_x;
	ret->y = initial_y;
	ret->w = initial_w;
	ret->h = initial_h;
	ret->colour = colour;

	return ret;
}

typedef struct text {
	char *str; /**< the content oof text */
	signed short x; /**< X pixel coord of ball on screen */
	signed short y; /**< Y pixel coord of ball on screen */
	unsigned int colour; /**< Hex RGB colour of the ball */
} text_t;

text_t *createText(char *str, signed short initial_x, signed short initial_y,
		   unsigned int colour)
{
	text_t *ret = calloc(1, sizeof(text_t));
	if (!ret) {
		fprintf(stderr, "Create text failed!");
		exit(EXIT_FAILURE);
	}

	ret->str = str;
	ret->x = initial_x;
	ret->y = initial_y;
	ret->colour = colour;

	return ret;
}

void updateCirclePosition(circle_t *circle, unsigned int count)
{
	float update_interval = count % 360 * 1 / (2 * M_PI);
	circle->x = CIRCLE_X + (SCREEN_WIDTH / 6 -
				(SCREEN_WIDTH / 6) * cos(update_interval));
	circle->y = CIRCLE_Y - (SCREEN_WIDTH / 6) * sin(update_interval);
	circle->x = round(circle->x);
	circle->y = round(circle->y);
}

void updateSquarePosition(square_t *square, unsigned int count)
{
	float update_interval = count % 360 * 1 / (2 * M_PI);
	square->x = (SCREEN_WIDTH * 2 / 3 -
		     (SCREEN_WIDTH / 6 -
		      (SCREEN_WIDTH / 6) * cos(update_interval))) -
		    SQUARE_SIDE_LENGTH / 2;
	square->y = (SCREEN_HEIGHT / 2 +
		     (SCREEN_WIDTH / 6) * sin(update_interval)) -
		    SQUARE_SIDE_LENGTH / 2;
	square->x = round(square->x);
	square->y = round(square->y);
}

void updateTextPositionToRight(text_t *text, signed int position_x,
			       unsigned int count)
{
	static int text_width = 0;
	float update_interval = count % 360 * 2 / (2 * M_PI);
	if (!tumGetTextSize((char *)text, &text_width, NULL)) {
		text->x = position_x +
			  (SCREEN_WIDTH / 6 -
			   (SCREEN_WIDTH / 6) * cos(update_interval));
	}
	text->x = round(text->x);
}

void updateTextPositionToLeft(text_t *text, signed int position_x,
			      unsigned int count)
{
	static int text_width = 0;
	float update_interval = count % 360 * 2 / (2 * M_PI);
	if (!tumGetTextSize((char *)text, &text_width, NULL)) {
		text->x = position_x -
			  (SCREEN_WIDTH / 6 -
			   (SCREEN_WIDTH / 6) * cos(update_interval));
	}
	text->x = round(text->x);
}

typedef struct buttons_buffer {
	unsigned char buttons[SDL_NUM_SCANCODES];
	SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void xGetButtonInput(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
		xSemaphoreGive(buttons.lock);
	}
}

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

void vDemoTask(void *pvParameters)
{
	unsigned int count = 0;

	circle_t *my_circle =
		createCircle(CIRCLE_X, CIRCLE_Y, CIRCLE_RADIUS, TUMBlue);

	square_t *my_square =
		createSquare(SCREEN_WIDTH * 2 / 3 - SQUARE_SIDE_LENGTH / 2,
			     SCREEN_HEIGHT / 2 - SQUARE_SIDE_LENGTH / 2,
			     SQUARE_SIDE_LENGTH, SQUARE_SIDE_LENGTH, Green);

	// Create strings
	static char our_string1[100];
	static char our_string2[100];
	static int our_string1_width = 0;
	static int our_string2_width = 0;
	static char our_string3[100];
	static char our_string4[100];
	static int our_string3_width = 0;
	static int our_string4_width = 0;
	text_t *my_text3 = createText(
		our_string3, SCREEN_WIDTH / 3 - our_string3_width / 2,
		SCREEN_HEIGHT / 2 - SQUARE_SIDE_LENGTH + DEFAULT_FONT_SIZE / 2,
		Black);
	text_t *my_text4 = createText(
		our_string4, SCREEN_WIDTH * 2 / 3 - our_string4_width / 2,
		SCREEN_HEIGHT / 2 - SQUARE_SIDE_LENGTH + DEFAULT_FONT_SIZE / 2,
		Black);

	// Needed such that Gfx library knows which thread controlls drawing
	// Only one thread can call tumDrawUpdateScreen while and thread can call
	// the drawing functions to draw objects. This is a limitation of the SDL
	// backend.
	tumDrawBindThread();

	while (1) {
		tumEventFetchEvents(
			FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
		xGetButtonInput(); // Update global input

		// `buttons` is a global shared variable and as such needs to be
		// guarded with a mutex, mutex must be obtained before accessing the
		// resource and given back when you're finished. If the mutex is not
		// given back then no other task can access the reseource.
		if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
			if (buttons.buttons[KEYCODE(
				    Q)]) { // Equiv to SDL_SCANCODE_Q
				exit(EXIT_SUCCESS);
			}
			xSemaphoreGive(buttons.lock);
		}

		tumDrawClear(White); // Clear screen

		// clock_gettime(CLOCK_REALTIME,
		//                &the_time); // Get kernel real time

		// Format our string into our char array
		sprintf(our_string1, "Static1");
		sprintf(our_string2, "Static2");

		sprintf(our_string3, "Dynamic3");
		sprintf(our_string4, "Dynamic4");

		if (!tumGetTextSize((char *)our_string1, &our_string1_width,
				    NULL))
			tumDrawText(our_string1,
				    SCREEN_WIDTH / 3 - our_string1_width / 2,
				    SCREEN_HEIGHT / 2 + SQUARE_SIDE_LENGTH -
					    DEFAULT_FONT_SIZE / 2,
				    Black);

		if (!tumGetTextSize((char *)our_string2, &our_string2_width,
				    NULL))
			tumDrawText(our_string2,
				    SCREEN_WIDTH * 2 / 3 -
					    our_string2_width / 2,
				    SCREEN_HEIGHT / 2 + SQUARE_SIDE_LENGTH -
					    DEFAULT_FONT_SIZE / 2,
				    Black);
		printf("width1: %d \n", our_string4_width);
		// Let my_text3 move
		updateTextPositionToRight(
			my_text3, SCREEN_WIDTH / 3 - our_string3_width / 2,
			count);
		printf("width2: %d \n", our_string3_width);
		if (!tumGetTextSize((char *)our_string3, &our_string3_width,
				    NULL))
			tumDrawText(my_text3->str, my_text3->x, my_text3->y,
				    my_text3->colour);

		// Let my_text4 move
		updateTextPositionToLeft(
			my_text4, SCREEN_WIDTH * 2 / 3 - our_string4_width / 2,
			count);
		if (!tumGetTextSize((char *)our_string4, &our_string4_width,
				    NULL))
			tumDrawText(my_text4->str, my_text4->x, my_text4->y,
				    my_text4->colour);

		// Draw the triangle
		tumDrawTriangle(points, Red);

		// Let circle rotate around the triangle
		updateCirclePosition(my_circle, count);
		tumDrawCircle(my_circle->x, my_circle->y, my_circle->colour,
			      my_circle->radius);

		// Let circle rotate around the triangle
		updateSquarePosition(my_square, count);
		tumDrawFilledBox(my_square->x, my_square->y, my_square->w,
				 my_square->h, my_square->colour);

		tumDrawUpdateScreen(); // Refresh the screen to draw string

		// Basic sleep of 1000 milliseconds
		count++;
		vTaskDelay((TickType_t)100);
	}
}

int main(int argc, char *argv[])
{
	char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

	printf("Initializing: ");

	if (tumDrawInit(bin_folder_path)) {
		PRINT_ERROR("Failed to initialize drawing");
		goto err_init_drawing;
	}

	if (tumEventInit()) {
		PRINT_ERROR("Failed to initialize events");
		goto err_init_events;
	}

	if (tumSoundInit(bin_folder_path)) {
		PRINT_ERROR("Failed to initialize audio");
		goto err_init_audio;
	}

	buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
	if (!buttons.lock) {
		PRINT_ERROR("Failed to create buttons lock");
		goto err_buttons_lock;
	}

	if (xTaskCreate(vDemoTask, "DemoTask", mainGENERIC_STACK_SIZE * 2, NULL,
			mainGENERIC_PRIORITY, &DemoTask) != pdPASS) {
		goto err_demotask;
	}

	vTaskStartScheduler();

	return EXIT_SUCCESS;

err_demotask:
	vSemaphoreDelete(buttons.lock);
err_buttons_lock:
	tumSoundExit();
err_init_audio:
	tumEventExit();
err_init_events:
	tumDrawExit();
err_init_drawing:
	return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
	/* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
	struct timespec xTimeToSleep, xTimeSlept;
	/* Makes the process more agreeable when using the Posix simulator. */
	xTimeToSleep.tv_sec = 1;
	xTimeToSleep.tv_nsec = 0;
	nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
