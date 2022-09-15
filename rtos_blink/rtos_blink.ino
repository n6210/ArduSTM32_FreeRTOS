#include <MapleFreeRTOS900.h>
#include <limits.h>
#include <utility/timers.h>

#define UNUSED __attribute__((unused))
#define GEN_PIN PB8
#define INPUT_PIN PB7

SemaphoreHandle_t interruptSemaphore;
TaskHandle_t printHandle;
TimerHandle_t xTimer = NULL;
volatile uint32_t count = 0;
volatile TickType_t xTime = configTICK_RATE_HZ;
volatile uint32_t lTime = 0;

static void print_time(void) {
  taskENTER_CRITICAL();
  uint8 s = lTime % 60;
  uint8 m = (lTime / 60) % 60;
  uint16 h = (lTime / 3600) % 3600;
  taskEXIT_CRITICAL();

  printf("Time: %02d:%02d:%02d\n", h, m, s);
}

// Timer callback
static void vTimeCB(UNUSED TimerHandle_t xTimer) {
  lTime++;
}

// Timer callback
static void vIncTimerCB(UNUSED TimerHandle_t xTimer) {
  static uint32 freq = 0;

  freq += 5;
  if (freq > 70) freq = 5;

  taskENTER_CRITICAL();
  xTime = (configTICK_RATE_HZ / freq) / 2;
  taskEXIT_CRITICAL();

  //printf("Gen f=%lu (%lu)\n", freq, xTime);
}

// Task - freq gen
static void vGenTask(UNUSED void *pvParameters) {
  while (1) {
    vTaskDelay(xTime);
    digitalWrite(GEN_PIN, !digitalRead(GEN_PIN));
  }
}

// Task - send data and clear counter
static void vClrTask(UNUSED void *pvParameters) {
  while (1) {
    xTaskNotifyAndQuery(printHandle, count, eSetValueWithOverwrite, NULL);
    count = 0;
    print_time();
    vTaskDelay(configTICK_RATE_HZ);
  }
}

// Task - print recive data and print counter value
static void vPrintTask(UNUSED void *pvParameters) {
  uint32_t val = 0;

  while (1) {
    //xTaskNotifyWait(0, ULONG_MAX, &val, configTICK_RATE_HZ);
    xTaskNotifyWait(0, ULONG_MAX, &val, portMAX_DELAY);
    printf("Count: %ld\n", val);
  }
}

// Interrupt handler -
void interruptHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  //count++;
  xSemaphoreGiveFromISR(interruptSemaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  //xTaskNotifyFromISR(printHandle, count, eSetBits, &xHigherPriorityTaskWoken);
}

// Task - blink LED according to the input
static void vLedTask(UNUSED void *pvParameters) {
  while (1) {
    if (xSemaphoreTake(interruptSemaphore, portMAX_DELAY) == pdPASS) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
}

// General setup
void setup() {
  // initialize the digital pin as an output:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(GEN_PIN, OUTPUT);
  pinMode(INPUT_PIN, INPUT);

  Serial.begin(115200);
  // wait for console connection
  while (!Serial) {}

  xTaskCreate(vGenTask, "Gen", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vLedTask, "Led", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vPrintTask, "Prt", configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 1, &printHandle);
  xTaskCreate(vClrTask, "Clr", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

  interruptSemaphore = xSemaphoreCreateBinary();
  if (interruptSemaphore != NULL) {
    // Attach interrupt for Arduino digital pin
    attachInterrupt(digitalPinToInterrupt(INPUT_PIN), interruptHandler, FALLING);
  }

  // Create timer to change LED frequency
  xTimer = xTimerCreate("IncTimer", pdMS_TO_TICKS(3000), pdTRUE, NULL, vIncTimerCB);
  if (xTimer != NULL) {
    if (xTimerStart(xTimer, 0) != pdPASS) {
      Serial.print(F("Timer is not started"));
    }
  }

  // Create timer to count time
  TimerHandle_t xTimeCnt = xTimerCreate("Time", configTICK_RATE_HZ, pdTRUE, NULL, vTimeCB);
  if (xTimeCnt != NULL) {
    xTimerStart(xTimeCnt, 0);
  }

  // System start
  vTaskStartScheduler();
}

// Will be run inside the IdleTask
void loop() {
  count++;
}

extern "C" {
  // Run loop() in a background
  void vApplicationIdleHook(void) {
    loop();
  }

  // Define putchar for printf()
  int _putchar(char c) {
    return Serial.print(c);
  }
}