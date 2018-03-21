################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/ble.c \
../src/bluetooth.c \
../src/hci.c \
../src/main.c \
../src/sdp.c \
../src/uuid.c 

OBJS += \
./src/ble.o \
./src/bluetooth.o \
./src/hci.o \
./src/main.o \
./src/sdp.o \
./src/uuid.o 

C_DEPS += \
./src/ble.d \
./src/bluetooth.d \
./src/hci.d \
./src/main.d \
./src/sdp.d \
./src/uuid.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	arm-linux-gnueabihf-gcc -I/home/liangjf/eclipse-workspace/ble_socket/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


