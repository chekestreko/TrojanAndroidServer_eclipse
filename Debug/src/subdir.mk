################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/ConnectionProxy.cpp \
../src/Journal.cpp \
../src/NetUtils.cpp \
../src/TrojanTime.cpp \
../src/client.cpp \
../src/clients.cpp \
../src/trojan_srv.cpp 

OBJS += \
./src/ConnectionProxy.o \
./src/Journal.o \
./src/NetUtils.o \
./src/TrojanTime.o \
./src/client.o \
./src/clients.o \
./src/trojan_srv.o 

CPP_DEPS += \
./src/ConnectionProxy.d \
./src/Journal.d \
./src/NetUtils.d \
./src/TrojanTime.d \
./src/client.d \
./src/clients.d \
./src/trojan_srv.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D__cplusplus=201703L -O0 -g3 -Wall -c -fmessage-length=0 -std=c++17 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


