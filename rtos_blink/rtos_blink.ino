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

static void vTimeCB(UNUSED TimerHandle_t xTimer) {
  static uint32_t time = 0;
  uint8 s = time % 60;
  uint8 m = (time / 60) % 60;
  uint16 h = (time / 3600) % 3600;
  time += 0xF;
  printf("Time: %02d:%02d:%02d\n", h, m, s);
  if (time >= 0xFFFFFFF0)
    vTaskEndScheduler();
}

static void vIncTimerCB(UNUSED TimerHandle_t xTimer) {
  static uint32 freq = 0;

  freq += 5;
  if (freq > 70) freq = 5;

  taskENTER_CRITICAL();
  xTime = (configTICK_RATE_HZ / freq) / 2;
  taskEXIT_CRITICAL();

  printf("Gen f=%lu (%lu)\n", freq, xTime);
}

static void vGenTask(UNUSED void *pvParameters) {
  while (1) {
    vTaskDelay(xTime);
    digitalWrite(GEN_PIN, !digitalRead(GEN_PIN));
  }
}

static void vClrTask(UNUSED void *pvParameters) {
  while (1) {
    xTaskNotifyAndQuery(printHandle, count, eSetValueWithOverwrite, NULL);
    count = 0;
    vTaskDelay(configTICK_RATE_HZ);
  }
}

static void vPrintTask(UNUSED void *pvParameters) {
  uint32_t val = 0;

  while (1) {
    //xTaskNotifyWait(0, ULONG_MAX, &val, configTICK_RATE_HZ);
    xTaskNotifyWait(0, ULONG_MAX, &val, portMAX_DELAY);
    printf("Count: %ld\n", val);
  }
}

void interruptHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  count++;
  xSemaphoreGiveFromISR(interruptSemaphore, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  //xTaskNotifyFromISR(printHandle, count, eSetBits, &xHigherPriorityTaskWoken);
}

static void vLedTask(UNUSED void *pvParameters) {
  while (1) {
    if (xSemaphoreTake(interruptSemaphore, portMAX_DELAY) == pdPASS) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
}

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

  xTimer = xTimerCreate("IncTimer", pdMS_TO_TICKS(3000), pdTRUE, NULL, vIncTimerCB);
  if (xTimer != NULL) {
    if (xTimerStart(xTimer, 0) != pdPASS) {
      Serial.print(F("Timer is not started"));
    }
  }
  TimerHandle_t xTimeCnt = xTimerCreate("Time", configTICK_RATE_HZ / 10, pdTRUE, NULL, vTimeCB);
  if (xTimeCnt != NULL) {
    xTimerStart(xTimeCnt, 0);
  }

  vTaskStartScheduler();
}

void loop() {}

extern "C" {
  int _putchar(char c) {
    return Serial.print(c);
  }
}