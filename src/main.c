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
#include "timers.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define PRINT_TASK_ERROR(task) PRINT_ERROR("Failed to print task ##task");

#define STACK_SIZE 200

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

#define NEXT_TASK 0
#define PREV_TASK 1

// Set all states
#define STATE_ONE 0
#define STATE_TWO 1
#define STATE_THREE 2
#define STATE_COUNT 3
#define STATE_QUEUE_LENGTH 1
#define STARTING_STATE STATE_ONE

// Set time
#define DEBOUNCE_DELAY 50 / 1000

// Set circle
#define CIRCLE_X SCREEN_WIDTH / 3
#define CIRCLE_Y SCREEN_HEIGHT / 2
#define CIRCLE_RADIUS 30
#define CIRCLE_LEFT_X SCREEN_WIDTH / 3 + SCREEN_WIDTH / 12
#define CIRCLE_LEFT_Y SCREEN_HEIGHT / 2
#define CIRCLE_RIGHT_X SCREEN_WIDTH * 2 / 3 - SCREEN_WIDTH / 12
#define CIRCLE_RIGHT_Y SCREEN_HEIGHT / 2

// Set triangle
#define TRIANGLE_SIDE_LENGTH 60
#define CORNER_1_X SCREEN_WIDTH / 2
#define CORNER_1_Y SCREEN_HEIGHT / 2 - TRIANGLE_SIDE_LENGTH *sqrt(3) / 4
#define CORNER_2_X SCREEN_WIDTH / 2 - TRIANGLE_SIDE_LENGTH / 2
#define CORNER_2_Y SCREEN_HEIGHT / 2 + TRIANGLE_SIDE_LENGTH *sqrt(3) / 4
#define CORNER_3_X SCREEN_WIDTH / 2 + TRIANGLE_SIDE_LENGTH / 2
#define CORNER_3_Y SCREEN_HEIGHT / 2 + TRIANGLE_SIDE_LENGTH *sqrt(3) / 4

// Set square
#define SQUARE_SIDE_LENGTH 50

// Set button
#define BUTTON_PRESSED 1
#define BUTTON_UNPRESSED 0

// Set coord for buttons
#define BUTTON_TEXT_A_X 20
#define BUTTON_TEXT_A_Y 40
#define BUTTON_TEXT_B_X 80
#define BUTTON_TEXT_B_Y 40
#define BUTTON_TEXT_C_X 140
#define BUTTON_TEXT_C_Y 40
#define BUTTON_TEXT_D_X 200
#define BUTTON_TEXT_D_Y 40
#define BUTTON_TEXT_T_X 440
#define BUTTON_TEXT_T_Y 60
#define BUTTON_TEXT_U_X 490
#define BUTTON_TEXT_U_Y 60

// Set coord for text to draw
#define INFO_TEXT_X 420
#define INFO_TEXT_Y 20
#define INFO_TASK3_X 320
#define INFO_TASK3_Y 40
#define INFO_TASK4_X 320
#define INFO_TASK4_Y 80
#define STATE_TASK4_X 410
#define STATE_TASK4_Y 80
#define MOUSE_TEXT_X 20
#define MOUSE_TEXT_Y 20
#define RESET_TEXT_X 320
#define RESET_TEXT_Y 60

// Set coord for text to draw (EX4)
#define STRING_EX4_X 20
#define STRING_EX4_Y 20
#define TASK2_EX4_X 60
#define TASK2_EX4_Y 40
#define TASK3_EX4_X 100
#define TASK3_EX4_Y 60
#define TASK4_EX4_X 140
#define TASK4_EX4_Y 80

// PRIORITIES EX4
#define  PRIORITY_TASK1EX4 1
#define  PRIORITY_TASK2EX4 2
#define  PRIORITY_TASK3EX4 3
#define  PRIORITY_TASK4EX4 4

// Task handle
static TaskHandle_t BufferSwap = NULL;
static TaskHandle_t StateMachine = NULL;
static TaskHandle_t DrawTask1 = NULL;
static TaskHandle_t DrawTask2a = NULL;
static TaskHandle_t DrawTask2b = NULL;
static TaskHandle_t ButtonTask3a = NULL;
static TaskHandle_t ButtonTask3b = NULL;
static TaskHandle_t ButtonTask4 = NULL;
static TimerHandle_t ResetButtonNum15sec = NULL;
static TaskHandle_t DrawScreenEx4 = NULL;
static TaskHandle_t DrawTask1Ex4 = NULL;
static TaskHandle_t DrawTask2Ex4 = NULL;
static TaskHandle_t DrawTask3Ex4 = NULL;
static TaskHandle_t DrawTask4Ex4 = NULL;



StackType_t xStack[STACK_SIZE];
StaticTask_t xTaskBuffer;

const unsigned char next_state_signal = NEXT_TASK;
const unsigned char prev_state_signal = PREV_TASK;

static QueueHandle_t StateQueue = NULL;

static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;
static SemaphoreHandle_t Button_T_Signal = NULL;
static SemaphoreHandle_t Button_U_Signal = NULL;
static SemaphoreHandle_t Reset15sec_T_Signal = NULL;
static SemaphoreHandle_t Reset15sec_U_Signal = NULL;
static SemaphoreHandle_t WakeUpTask3Ex4 = NULL;


static long int lastDebounceTime = 0;
static unsigned int reading = BUTTON_UNPRESSED;
static unsigned int GLBcount = 0;
static TickType_t StartTickCount = 0;
static TickType_t TickCount = 0;
static char stringEx4[20][50];

/*=======================FUNCTION========================*/
typedef struct circle {
	signed short x; /**< X pixel coord of ball on screen */
	signed short y; /**< Y pixel coord of ball on screen */
	unsigned short radius; /**< Radius of the ball in pixels */
	unsigned int colour; /**< Hex RGB colour of the ball */
} circle_t;

circle_t *createCircle(signed short initial_x, signed short initial_y,
		       unsigned short radius, unsigned int colour)
{
	circle_t *ret = calloc(1, sizeof(circle_t));

	if (!ret) {
		fprintf(stderr, "Creating circle failed\n");
		exit(EXIT_FAILURE);
	}

	ret->x = initial_x;
	ret->y = initial_y;
	ret->radius = radius;
	ret->colour = colour;

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

void updateTextPositionToLeft(text_t *text, signed int position_x,
			      unsigned int count)
{
	float update_interval = count % 360 * 2 / (2 * M_PI);
	text->x = position_x - (SCREEN_WIDTH / 6 -
				(SCREEN_WIDTH / 6) * cos(update_interval));
	text->x = round(text->x);
}

void updateTextPositionToRight(text_t *text, signed int position_x,
			       unsigned int count)
{
	float update_interval = count % 360 * 2 / (2 * M_PI);
	text->x = position_x + (SCREEN_WIDTH / 6 -
				(SCREEN_WIDTH / 6) * cos(update_interval));
	text->x = round(text->x);
}

typedef struct button {
	char *name;
	unsigned int SDL_CODE_NUM;
	unsigned int pressed_count;
	unsigned int state;
} button_t;

button_t *createButton(char *name, unsigned int SDL_CODE_NUM,
		       unsigned int count, unsigned int state)
{
	button_t *ret = calloc(1, sizeof(button_t));
	if (!ret) {
		fprintf(stderr, "Create button failed!");
		exit(EXIT_FAILURE);
	}

	ret->name = name;
	ret->SDL_CODE_NUM = SDL_CODE_NUM;
	ret->pressed_count = count;
	ret->state = state;
	return ret;
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

void updatePressedCount(button_t *button)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (button->state == BUTTON_PRESSED) {
			button->pressed_count += 1;
		}
		xSemaphoreGive(buttons.lock);
	}
}

void Handle_Debounce(button_t *button)
{
	static struct timespec the_time;
	clock_gettime(CLOCK_REALTIME,
		      &the_time); // Get kernel real time
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (buttons.buttons[button->SDL_CODE_NUM]) {
			reading = BUTTON_PRESSED;
		} else {
			reading = BUTTON_UNPRESSED;
		}
		xSemaphoreGive(buttons.lock);
	}
	if (reading != button->state) {
		lastDebounceTime = the_time.tv_sec;
	}
	if ((the_time.tv_sec - lastDebounceTime) >= DEBOUNCE_DELAY) {
		if (reading != button->state) {
			button->state = reading;
			updatePressedCount(button);
		}
	}
}

void Handle_Button(button_t *button, unsigned short button_X,
		   unsigned short button_Y)
{
	static char buttonText[100];
	static int buttonText_width = 0;
	Handle_Debounce(button); // debounce input of button

	sprintf(buttonText, "%s: %u |", button->name, button->pressed_count);
	if (!tumGetTextSize((char *)buttonText, &buttonText_width, NULL))
		tumDrawText(buttonText, button_X, button_Y, Black);
}

void DrawInfoText(int shift_x, int shift_y)
{
	static char our_string[100];
	static int our_string_width = 0;
	sprintf(our_string, "[Q] to [Q]uit | [E] to Chang[e]");
	if (!tumGetTextSize((char *)our_string, &our_string_width, NULL))
		tumDrawText(our_string, INFO_TEXT_X + shift_x,
			    INFO_TEXT_Y + shift_y, Red);
}

void DrawInfoTask3()
{
	static char our_string[100];
	static int our_string_width = 0;
	sprintf(our_string, "[T] to count number T | [U] to count number U");
	if (!tumGetTextSize((char *)our_string, &our_string_width, NULL))
		tumDrawText(our_string, INFO_TASK3_X, INFO_TASK3_Y, Black);
}

void DrawTextTask4(unsigned int count)
{
	static char our_string[100];
	static int our_string_width = 0;
	sprintf(our_string, "Counter: %d ",count);
	if (!tumGetTextSize((char *)our_string, &our_string_width, NULL))
		tumDrawText(our_string, INFO_TASK4_X, INFO_TASK4_Y, Black);
}

void DrawStateTask4(char *str)
{
	static char our_string4[100];
	static int our_string_width4 = 0;
	sprintf(our_string4, "%s", str);
	if (!tumGetTextSize((char *)our_string4, &our_string_width4, NULL)) {
		tumDrawText(our_string4, STATE_TASK4_X, STATE_TASK4_Y, Red);
	}
}

void DrawResetText()
{
	static char our_string[100];
	static int our_string_width = 0;
	sprintf(our_string, "(Reset after 15s)");
	if (!tumGetTextSize((char *)our_string, &our_string_width, NULL))
		tumDrawText(our_string, RESET_TEXT_X, RESET_TEXT_Y, Red);
}


void DrawText(unsigned count, int shift_x, int shift_y)
{
	static char our_string1[100];
	static char our_string2[100];
	static int our_string1_width = 0;
	static int our_string2_width = 0;
	static char our_string3[100];
	static char our_string4[100];
	static int our_string3_width = 0;
	static int our_string4_width = 0;

	sprintf(our_string1, "Static1");
	sprintf(our_string2, "Static2");
	sprintf(our_string3, "Dynamic3");
	sprintf(our_string4, "Dynamic4");

	text_t *my_text3 = createText(
		our_string3, SCREEN_WIDTH / 3 - our_string3_width / 2,
		SCREEN_HEIGHT / 2 - SQUARE_SIDE_LENGTH + DEFAULT_FONT_SIZE / 2,
		Black);
	text_t *my_text4 = createText(
		our_string4, SCREEN_WIDTH * 2 / 3 - our_string4_width / 2,
		SCREEN_HEIGHT / 2 - SQUARE_SIDE_LENGTH + DEFAULT_FONT_SIZE / 2,
		Black);
	if (!tumGetTextSize((char *)our_string1, &our_string1_width, NULL))
		tumDrawText(our_string1,
			    SCREEN_WIDTH / 3 - our_string1_width / 2 + shift_x,
			    SCREEN_HEIGHT / 2 + SQUARE_SIDE_LENGTH -
				    DEFAULT_FONT_SIZE / 2 + shift_y,
			    Black);

	if (!tumGetTextSize((char *)our_string2, &our_string2_width, NULL))
		tumDrawText(our_string2,
			    SCREEN_WIDTH * 2 / 3 - our_string2_width / 2 +
				    shift_x,
			    SCREEN_HEIGHT / 2 + SQUARE_SIDE_LENGTH -
				    DEFAULT_FONT_SIZE / 2 + shift_y,
			    Black);

	// Let my_text4 move

	updateTextPositionToLeft(my_text4,
				 (SCREEN_WIDTH * 2 / 3 - our_string4_width / 2),
				 count);

	if (!tumGetTextSize((char *)our_string4, &our_string4_width, NULL))
		tumDrawText(my_text4->str, my_text4->x + shift_x,
			    my_text4->y + shift_y, my_text4->colour);

	// Let my_text3 move
	updateTextPositionToRight(
		my_text3, SCREEN_WIDTH / 3 - our_string3_width / 2, count);

	if (!tumGetTextSize((char *)our_string3, &our_string3_width, NULL))
		tumDrawText(my_text3->str, my_text3->x + shift_x,
			    my_text3->y + shift_y, my_text3->colour);
}

void clickMouseToResetButtons(button_t *button, unsigned short button_X,
			      unsigned short button_Y)
{
	if (tumEventGetMouseLeft()) {
		static char buttonText[100];
		static int buttonText_width = 0;
		button->pressed_count = 0;
		sprintf(buttonText, "%s: %u |", button->name,
			button->pressed_count);
		if (!tumGetTextSize((char *)buttonText, &buttonText_width,
				    NULL))
			tumDrawText(buttonText, button_X, button_Y, Black);
	}
}

typedef struct mouseInfo {
	int mouseX; // The mouse's recent X coorde (in pixels)
	int mouseY; // The mouse's recent Y coorde (in pixels)
} mouseInfo_t;

mouseInfo_t *createMouseInfo(int mouseX, int mouseY)
{
	mouseInfo_t *ret = calloc(1, sizeof(mouseInfo_t));
	if (!ret) {
		fprintf(stderr, "Create mouseInfo failed!");
		exit(EXIT_FAILURE);
	}

	ret->mouseX = mouseX;
	ret->mouseY = mouseY;
	return ret;
}

void getMouseInfo(mouseInfo_t *mouse)
{
	mouse->mouseX = tumEventGetMouseX();
	mouse->mouseY = tumEventGetMouseY();
}

void printMouseInfo(mouseInfo_t *mouse, unsigned short text_X,
		    unsigned short text_Y)
{
	static char Text[100];
	static int Text_width = 0;
	getMouseInfo(mouse);

	sprintf(Text, "Axis_X: %d |Axis_X: %d", mouse->mouseX, mouse->mouseY);
	if (!tumGetTextSize((char *)Text, &Text_width, NULL))
		tumDrawText(Text, text_X, text_Y, Black);
}

void handle_coord(mouseInfo_t *mouse)
{
	getMouseInfo(mouse);
	mouse->mouseX = (mouse->mouseX - SCREEN_WIDTH / 2) / 10;
	mouse->mouseY = (mouse->mouseY - SCREEN_HEIGHT / 2) / 10;
}

static int vCheckStateInput(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		if (buttons.buttons[KEYCODE(E)]) {
			buttons.buttons[KEYCODE(E)] = 0;
			if (StateQueue) {
				xSemaphoreGive(buttons.lock);
				xQueueSend(StateQueue, &next_state_signal, 0);
				return 0;
			}
			return -1;
		}
		xSemaphoreGive(buttons.lock);
	}

	return 0;
}

void changeState(volatile unsigned char *state, unsigned char forwards)
{
	switch (forwards) {
	case NEXT_TASK:
		if (*state == STATE_COUNT - 1) {
			*state = 0;
		} else {
			(*state)++;
		}
		break;
	case PREV_TASK:
		if (*state == 0) {
			*state = STATE_COUNT - 1;
		} else {
			(*state)--;
		}
		break;
	default:
		break;
	}
}

/*=========================TASKS==========================*/
void vTimerCallbackReset15sec (xTimerHandle pxTimer) {
	xSemaphoreGive(Reset15sec_T_Signal);
	xSemaphoreGive(Reset15sec_U_Signal);
}
void vSwapBuffers(void *pvParameters)
{
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	const TickType_t frameratePeriod = 50;
	tumDrawBindThread(); // Setup Rendering handle with correct GL context

	while (1) {
		if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
			tumDrawUpdateScreen();
			tumEventFetchEvents(FETCH_EVENT_BLOCK);
			xSemaphoreGive(ScreenLock);
			xSemaphoreGive(DrawSignal);
			vTaskDelayUntil(&xLastWakeTime,
					pdMS_TO_TICKS(frameratePeriod));
		}
	}
}

void basicSequentialStateMachine(void *pvParameters)
{
	unsigned char current_state = STARTING_STATE; // Default state
	unsigned char state_changed = 1;
	unsigned char input = 0;

	while (1) {
		if (state_changed) {
			goto initial_state;
		}
		if (StateQueue)
			if (xQueueReceive(StateQueue, &input, portMAX_DELAY) ==
			    pdTRUE) {
				changeState(&current_state, input);
				state_changed = 1;
			}

	initial_state:
		// Handle current state
		if (state_changed) {
			if (state_changed) {
				switch (current_state) {
				case STATE_ONE:
					if (DrawTask2a) {
						vTaskSuspend(DrawTask2a);
						vTaskSuspend(DrawTask2b);
						vTaskSuspend(ButtonTask4);
						xTimerStop(ResetButtonNum15sec, 0);
					}
					if (DrawTask2b) {
						vTaskSuspend(DrawTask2a);
						vTaskSuspend(DrawTask2b);
						vTaskSuspend(ButtonTask4);
						xTimerStop(ResetButtonNum15sec, 0);
					}
					if (DrawTask1Ex4) {
						vTaskSuspend(DrawScreenEx4);
						vTaskSuspend(DrawTask1Ex4);
						vTaskSuspend(DrawTask2Ex4);
						vTaskSuspend(DrawTask3Ex4);
						vTaskSuspend(DrawTask4Ex4);
					}
					if (DrawTask1) {
						vTaskResume(DrawTask1);
					}
					
					break;
				case STATE_TWO:
					if (DrawTask1) {
						vTaskSuspend(DrawTask1);
					}
					if (DrawTask1Ex4) {
						vTaskSuspend(DrawScreenEx4);
						vTaskSuspend(DrawTask1Ex4);
						vTaskSuspend(DrawTask2Ex4);
						vTaskSuspend(DrawTask3Ex4);
						vTaskSuspend(DrawTask4Ex4);
					}
					if (DrawTask2a) {
						vTaskResume(DrawTask2a);
						vTaskResume(DrawTask2b);
						vTaskResume(ButtonTask4);
						xTimerStart(ResetButtonNum15sec, 0);
					}
					if (DrawTask2b) {
						vTaskResume(DrawTask2a);
						vTaskResume(DrawTask2b);
						vTaskResume(ButtonTask4);
						xTimerStart(ResetButtonNum15sec, 0);
					}
					break;
				case STATE_THREE:
					if (DrawTask2a) {
						vTaskSuspend(DrawTask2a);
						vTaskSuspend(DrawTask2b);
						vTaskSuspend(ButtonTask4);
						xTimerStop(ResetButtonNum15sec, 0);
					}
					if (DrawTask2b) {
						vTaskSuspend(DrawTask2a);
						vTaskSuspend(DrawTask2b);
						vTaskSuspend(ButtonTask4);
						xTimerStop(ResetButtonNum15sec, 0);
					}
					if (DrawTask1) {
						vTaskSuspend(DrawTask1);
					}
					if (DrawTask1Ex4) {
						StartTickCount = xTaskGetTickCount();
						vTaskResume(DrawScreenEx4);
						vTaskResume(DrawTask1Ex4);
						vTaskResume(DrawTask2Ex4);
						vTaskResume(DrawTask3Ex4);
						vTaskResume(DrawTask4Ex4);
					}
					break;
				default:
					break;
				}
				state_changed = 0;
			}
		}
	}
}

void vDrawTask1(void *pvParameters)
{
	unsigned int count = 0;
	circle_t *my_circle =
		createCircle(CIRCLE_X, CIRCLE_Y, CIRCLE_RADIUS, TUMBlue);

	square_t *my_square =
		createSquare(SCREEN_WIDTH * 2 / 3 - SQUARE_SIDE_LENGTH / 2,
			     SCREEN_HEIGHT / 2 - SQUARE_SIDE_LENGTH / 2,
			     SQUARE_SIDE_LENGTH, SQUARE_SIDE_LENGTH, Green);

	button_t *button_A =
		createButton("A", SDL_SCANCODE_A, 0, BUTTON_UNPRESSED);
	button_t *button_B =
		createButton("B", SDL_SCANCODE_B, 0, BUTTON_UNPRESSED);
	button_t *button_C =
		createButton("C", SDL_SCANCODE_C, 0, BUTTON_UNPRESSED);
	button_t *button_D =
		createButton("D", SDL_SCANCODE_D, 0, BUTTON_UNPRESSED);

	mouseInfo_t *mouse = createMouseInfo(0, 0);
	mouseInfo_t *mouseToShift = createMouseInfo(0, 0);

	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				xGetButtonInput(); // Update global input
				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
					if (buttons.buttons[KEYCODE(
						    Q)]) { // Equiv to SDL_SCANCODE_Q
						exit(EXIT_SUCCESS);
					}
					xSemaphoreGive(buttons.lock);
				}

				tumDrawClear(White); // Clear screen

				handle_coord(mouseToShift);

				DrawInfoText(mouseToShift->mouseX,
					     mouseToShift->mouseY);

				printMouseInfo(
					mouse,
					MOUSE_TEXT_X + mouseToShift->mouseX,
					MOUSE_TEXT_Y + mouseToShift->mouseY);

				coord_t points[3] = {
					{ CORNER_1_X + mouseToShift->mouseX,
					  CORNER_1_Y + mouseToShift->mouseY },
					{ CORNER_2_X + mouseToShift->mouseX,
					  CORNER_2_Y + mouseToShift->mouseY },
					{ CORNER_3_X + mouseToShift->mouseX,
					  CORNER_3_Y + mouseToShift->mouseY }
				};

				Handle_Button(
					button_A,
					BUTTON_TEXT_A_X + mouseToShift->mouseX,
					BUTTON_TEXT_A_Y + mouseToShift->mouseY);
				Handle_Button(
					button_B,
					BUTTON_TEXT_B_X + mouseToShift->mouseX,
					BUTTON_TEXT_B_Y + mouseToShift->mouseY);
				Handle_Button(
					button_C,
					BUTTON_TEXT_C_X + mouseToShift->mouseX,
					BUTTON_TEXT_C_Y + mouseToShift->mouseY);
				Handle_Button(
					button_D,
					BUTTON_TEXT_D_X + mouseToShift->mouseX,
					BUTTON_TEXT_D_Y + mouseToShift->mouseY);

				clickMouseToResetButtons(
					button_A,
					BUTTON_TEXT_A_X + mouseToShift->mouseX,
					BUTTON_TEXT_A_Y + mouseToShift->mouseY);
				clickMouseToResetButtons(
					button_B,
					BUTTON_TEXT_B_X + mouseToShift->mouseX,
					BUTTON_TEXT_B_Y + mouseToShift->mouseY);
				clickMouseToResetButtons(
					button_C,
					BUTTON_TEXT_C_X + mouseToShift->mouseX,
					BUTTON_TEXT_C_Y + mouseToShift->mouseY);
				clickMouseToResetButtons(
					button_D,
					BUTTON_TEXT_D_X + mouseToShift->mouseX,
					BUTTON_TEXT_D_Y + mouseToShift->mouseY);

				// Draw some text
				DrawText(count, mouseToShift->mouseX,
					 mouseToShift->mouseY);

				// Draw the triangle
				tumDrawTriangle(points, Red);

				// Let circle rotate around the triangle
				updateCirclePosition(my_circle, count);
				tumDrawCircle(
					my_circle->x + mouseToShift->mouseX,
					my_circle->y + mouseToShift->mouseY,
					my_circle->radius, my_circle->colour);

				// Let circle rotate around the triangle
				updateSquarePosition(my_square, count);
				tumDrawFilledBox(
					my_square->x + mouseToShift->mouseX,
					my_square->y + mouseToShift->mouseY,
					my_square->w, my_square->h,
					my_square->colour);

				// Basic sleep of 100 milliseconds

				count++;
				vTaskDelay((TickType_t)100);
				xSemaphoreGive(ScreenLock);

				vCheckStateInput();
			}
	}
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
				   StackType_t **ppxIdleTaskStackBuffer,
				   uint32_t *pulIdleTaskStackSize)
{
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}


void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vDrawTask2a(void *pvParameters)
{
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	char *info = "[S] to start/stop";

	circle_t *my_circle_left = createCircle(CIRCLE_LEFT_X, CIRCLE_LEFT_Y,
						CIRCLE_RADIUS, TUMBlue);

	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				tumDrawClear(White); // Clear screen
				xGetButtonInput();
				DrawStateTask4(info);
				DrawInfoText(0, 0);
				DrawResetText();
				DrawInfoTask3();
				DrawTextTask4(GLBcount);
				tumDrawCircle(my_circle_left->x,
					      my_circle_left->y,
					      my_circle_left->radius,
					      my_circle_left->colour);

				xSemaphoreGive(ScreenLock);
				vCheckStateInput();
				vTaskDelayUntil(&xLastWakeTime,
						500 / portTICK_RATE_MS);
			}
	}
}

void vDrawTask2b(void *pvParameters)
{
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	char *info = "[S] to start/stop";
	circle_t *my_circle_right = createCircle(CIRCLE_RIGHT_X, CIRCLE_RIGHT_Y,
						 CIRCLE_RADIUS, Red);
	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				
				tumDrawClear(White); // Clear screen
				xGetButtonInput();
				if (buttons.buttons[KEYCODE(S)]) {
					if (eTaskGetState(ButtonTask4) ==
					    eBlocked) {
						vTaskSuspend(ButtonTask4);				
					}
					else if (eTaskGetState(ButtonTask4) ==
						 eSuspended) {
						vTaskResume(ButtonTask4);
					}
				}
				if (buttons.buttons[KEYCODE(T)]) {
					xSemaphoreGive(Button_T_Signal);
				}
				if (buttons.buttons[KEYCODE(U)]) {
					xTaskNotifyGive(ButtonTask3b);
				}
				DrawStateTask4(info);
				DrawInfoText(0, 0);
				DrawInfoTask3();
				DrawTextTask4(GLBcount);
				DrawResetText();
				tumDrawCircle(my_circle_right->x,
					      my_circle_right->y,
					      my_circle_right->radius,
					      my_circle_right->colour);

				xSemaphoreGive(ScreenLock);
				vCheckStateInput();
				vTaskDelayUntil(&xLastWakeTime,
						250 / portTICK_RATE_MS);
			}
	}
}

void vButtonTask3a(void *pvParameters)
{
	button_t *button_T =
		createButton("T", SDL_SCANCODE_T, 0, BUTTON_UNPRESSED);
	while (1) {
		if (xSemaphoreTake(Reset15sec_T_Signal, 1)) {
			button_T->pressed_count = 0;
		}
		Handle_Button(button_T, BUTTON_TEXT_T_X,
				      BUTTON_TEXT_T_Y);
		if (xSemaphoreTake(Button_T_Signal, portMAX_DELAY)) {
			button_T->pressed_count += 1;
		}
		
	}

}

void vButtonTask3b(void *pvParameters)
{
	button_t *button_U =
		createButton("U", SDL_SCANCODE_U, 0, BUTTON_UNPRESSED);
	while (1) {
		if (xSemaphoreTake(Reset15sec_U_Signal, 1)) {
			button_U->pressed_count = 0;
		}
		Handle_Button(button_U, BUTTON_TEXT_U_X,
				      BUTTON_TEXT_U_Y);
		if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
			button_U->pressed_count += 1;
		}
		
	}
}

void vButtonTask4(void *pvParameters)
{
	while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
		GLBcount++;
	}
}

void vDrawScreenEx4(void *pvParameters)
{
	unsigned int i = 0;
	while (1) {
		if (DrawSignal)
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
			    pdTRUE) {
				tumEventFetchEvents(FETCH_EVENT_BLOCK |
						    FETCH_EVENT_NO_GL_CHECK);

				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				tumDrawClear(White); // Clear screen
				xGetButtonInput();
				DrawInfoText(0, 0);

				for (i = 0; i <= 14; i++) {
					tumDrawText(stringEx4[i], STRING_EX4_X,
						    STRING_EX4_Y + 20 * i,
						    TUMBlue);
				}

				xSemaphoreGive(ScreenLock);
				vCheckStateInput();
			}
	}
}

void vDrawTask1Ex4(void *pvParameters)
{
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	static char TextTask1Ex4[100];
	while (1) {
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount == 15){
					vTaskSuspend(DrawTask1Ex4);
					vTaskSuspend(DrawTask2Ex4);
					vTaskSuspend(DrawTask3Ex4);
					vTaskSuspend(DrawTask4Ex4);
				}
		vTaskDelayUntil(&xLastWakeTime, 1);
		xGetButtonInput();
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount <= 15){
					sprintf(TextTask1Ex4, "Tick%d : 1 |", TickCount);
					strcat(stringEx4[TickCount-1],TextTask1Ex4);
		}
		vCheckStateInput();
	}
}

void vDrawTask2Ex4(void *pvParameters)
{
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	static char TextTask2Ex4[100];
	while (1) {
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount == 15){
					vTaskSuspend(DrawTask1Ex4);
					vTaskSuspend(DrawTask2Ex4);
					vTaskSuspend(DrawTask3Ex4);
					vTaskSuspend(DrawTask4Ex4);
				}
		vTaskDelayUntil(&xLastWakeTime, 2);
		xSemaphoreGive(WakeUpTask3Ex4);
		xGetButtonInput();
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount <= 15){
					sprintf(TextTask2Ex4, "Tick%d : 2 |", TickCount);
					strcat(stringEx4[TickCount-1],TextTask2Ex4);
		}
		vCheckStateInput();
	}
}

void vDrawTask3Ex4(void *pvParameters)
{
	static char TextTask3Ex4[100];
	while (1) {
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount == 15){
					vTaskSuspend(DrawTask1Ex4);
					vTaskSuspend(DrawTask2Ex4);
					vTaskSuspend(DrawTask3Ex4);
					vTaskSuspend(DrawTask4Ex4);
				}
		if (xSemaphoreTake(WakeUpTask3Ex4, portMAX_DELAY)) {
			xGetButtonInput();
			TickCount = xTaskGetTickCount() - StartTickCount;
			if (TickCount <= 15){
					sprintf(TextTask3Ex4, "Tick%d : 3 |", TickCount);
					strcat(stringEx4[TickCount-1],TextTask3Ex4);
		}
			
			vCheckStateInput();
		}
	}
}

void vDrawTask4Ex4(void *pvParameters)
{
	TickType_t xLastWakeTime;
	static char TextTask4Ex4[100];
	xLastWakeTime = xTaskGetTickCount();
	while (1) {
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount == 15){
					vTaskSuspend(DrawTask1Ex4);
					vTaskSuspend(DrawTask2Ex4);
					vTaskSuspend(DrawTask3Ex4);
					vTaskSuspend(DrawTask4Ex4);
				}
		vTaskDelayUntil(&xLastWakeTime, 4);
		xGetButtonInput();
		TickCount = xTaskGetTickCount() - StartTickCount;
		if (TickCount <= 15){
					sprintf(TextTask4Ex4, "Tick%d : 4 |", TickCount);
					strcat(stringEx4[TickCount-1],TextTask4Ex4);
		}
		vCheckStateInput();
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

	DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
	if (!DrawSignal) {
		PRINT_ERROR("Failed to create draw signal");
	}

	Reset15sec_T_Signal = xSemaphoreCreateBinary(); // Screen buffer locking
	if (!DrawSignal) {
		PRINT_ERROR("Failed to create reset signal");
	}

	Reset15sec_U_Signal = xSemaphoreCreateBinary(); // Screen buffer locking
	if (!DrawSignal) {
		PRINT_ERROR("Failed to create reset signal");
	}

	WakeUpTask3Ex4 = xSemaphoreCreateBinary(); // Screen buffer locking
	if (!WakeUpTask3Ex4) {
		PRINT_ERROR("Failed to create wake up signal");
	}

	ScreenLock = xSemaphoreCreateMutex();
	if (!ScreenLock) {
		PRINT_ERROR("Failed to create screen lock");
	}

	Button_T_Signal =
		xSemaphoreCreateBinary(); // Give signal when button is pressed
	if (!Button_T_Signal) {
		PRINT_ERROR("Failed to create button T signal");
	}

	Button_U_Signal =
		xSemaphoreCreateBinary(); // Give signal when button is pressed
	if (!Button_U_Signal) {
		PRINT_ERROR("Failed to create button U signal");
	}

	StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
	if (!StateQueue) {
		PRINT_ERROR("Could not open state queue");
		goto err_state_queue;
	}

	if (xTaskCreate(vSwapBuffers, "BufferSwapTask",
			mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
			BufferSwap) != pdPASS) {
		PRINT_TASK_ERROR("BufferSwapTask");
	}

	if (xTaskCreate(basicSequentialStateMachine, "StateMachine",
			mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 1, &StateMachine) != pdPASS) {
		PRINT_TASK_ERROR("StateMachine");
	}

	if (xTaskCreate(vDrawTask1, "DrawTask1", mainGENERIC_STACK_SIZE * 2,
			NULL, configMAX_PRIORITIES, &DrawTask1) != pdPASS) {
		PRINT_TASK_ERROR("DrawTask1");
		goto err_demotask1;
	}

	DrawTask2a = xTaskCreateStatic(vDrawTask2a, "DrawTask2a", STACK_SIZE,
				       NULL, configMAX_PRIORITIES - 2, xStack,
				       &xTaskBuffer);

	if (xTaskCreate(vDrawTask2b, "DrawTask2b", mainGENERIC_STACK_SIZE * 2,
			NULL, configMAX_PRIORITIES - 1,
			&DrawTask2b) != pdPASS) {
		PRINT_TASK_ERROR("DrawTask2b");
	}

	if (xTaskCreate(vButtonTask3a, "ButtonTask3a",
			mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 2, &ButtonTask3a) != pdPASS) {
		PRINT_TASK_ERROR("ButtonTask3a");
	}

	if (xTaskCreate(vButtonTask3b, "ButtonTask3b",
			mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 3, &ButtonTask3b) != pdPASS) {
		PRINT_TASK_ERROR("ButtonTask3b");
	}

	ResetButtonNum15sec =
		xTimerCreate("reset15sec",
			     15000 / portTICK_PERIOD_MS, // Period time
			     pdTRUE, // Auto reload
			     (void *)0,
			     vTimerCallbackReset15sec); // Callback
	if (ResetButtonNum15sec == NULL) {
		for(;;); // Failure
	}
	if (xTimerStart(ResetButtonNum15sec,0) != pdPASS) {
		for(;;); // Failure
	}
             
	if (xTaskCreate(vButtonTask4, "ButtonTask4", mainGENERIC_STACK_SIZE * 2,
			NULL, configMAX_PRIORITIES - 5 , &ButtonTask4) != pdPASS) {
		PRINT_TASK_ERROR("ButtonTask4");
	}


	if (xTaskCreate(vDrawScreenEx4, "DrawScreenEx4", mainGENERIC_STACK_SIZE * 2,
			NULL, configMAX_PRIORITIES -1 , &DrawScreenEx4) != pdPASS) {
		PRINT_TASK_ERROR("DrawScreenEx4");
	}

	if (xTaskCreate(vDrawTask1Ex4, "DrawTask1Ex4", mainGENERIC_STACK_SIZE * 2,
			NULL, PRIORITY_TASK1EX4 , &DrawTask1Ex4) != pdPASS) {
		PRINT_TASK_ERROR("DrawTask1Ex4");
	}

	if (xTaskCreate(vDrawTask2Ex4, "DrawTask2Ex4", mainGENERIC_STACK_SIZE * 2,
			NULL, PRIORITY_TASK2EX4 , &DrawTask2Ex4) != pdPASS) {
		PRINT_TASK_ERROR("DrawTask2Ex4");
	}

	if (xTaskCreate(vDrawTask3Ex4, "DrawTask3Ex4", mainGENERIC_STACK_SIZE * 2,
			NULL, PRIORITY_TASK3EX4 , &DrawTask3Ex4) != pdPASS) {
		PRINT_TASK_ERROR("DrawTask3Ex4");
	}

	if (xTaskCreate(vDrawTask4Ex4, "DrawTask4Ex4", mainGENERIC_STACK_SIZE * 2,
			NULL, PRIORITY_TASK4EX4 , &DrawTask4Ex4) != pdPASS) {
		PRINT_TASK_ERROR("DrawTask4Ex4");
	}

	vTaskSuspend(DrawTask1);
    vTaskSuspend(DrawTask2a);
	vTaskSuspend(DrawTask2b);
	vTaskSuspend(DrawTask1Ex4);
	vTaskSuspend(DrawTask2Ex4);
	vTaskSuspend(DrawTask3Ex4);
	vTaskSuspend(DrawTask4Ex4);
	vTaskSuspend(DrawScreenEx4);
	xTimerStop(ResetButtonNum15sec, 0);

	vTaskStartScheduler();

	return EXIT_SUCCESS;

err_demotask1:
	vSemaphoreDelete(buttons.lock);
err_buttons_lock:
	tumSoundExit();
err_state_queue:
	vSemaphoreDelete(ScreenLock);
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
