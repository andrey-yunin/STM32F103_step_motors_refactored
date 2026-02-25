/*
 * task_motion_controller.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_motion_controller.h"
#include "main.h"
#include "cmsis_os.h"
#include "motion_planner.h" // Для MotorMotionState_t
#include "app_config.h"
#include "app_globals.h"
#include "app_queues.h"
#include "motion_driver.h"    // Объявляет функции MotionDriver_
#include "command_protocol.h" // Объявляет CMD_ константы и полную структуру MotionCommand_t


void app_start_task_motion_controller(void *argument)
{
	CAN_Command_t received_motion_cmd; // Buffer for received motion command

	// --- Initialization of motor states and motion driver ---
	for(uint8_t i = 0; i < MOTOR_COUNT; i++) {
		MotionPlanner_InitMotorState(&motor_states[i], 0); // All motors start at position 0
		}

	MotionDriver_Init(); // Initialize the new motion driver (e.g., disables all motors)

	// Main task loop
	for(;;)
	{
		// 1. Wait for a motion command in the motion_queue
		if (osMessageQueueGet(motion_queueHandle, &received_motion_cmd, NULL, osWaitForever) == osOK) {
			uint8_t motor_id = received_motion_cmd.motor_id;

			// --- Input Validation ---
			if (motor_id >= MOTOR_COUNT) {
				// Invalid motor ID, ignore command
				continue;
				}

			// Check if motor is already active
			if (g_motor_active[motor_id]) {
				// Motor is busy, ignore command for now.
				// Future improvement: queue commands or handle concurrent moves.
				continue;
				}

			// --- Process Command Type ---
			switch (received_motion_cmd.command_id) {
				case CMD_MOVE_ABSOLUTE:
				case CMD_MOVE_RELATIVE:
					{
						// 2. Calculate new target position and steps using motion planner
						int32_t target_pos;
						if (received_motion_cmd.command_id == CMD_MOVE_ABSOLUTE) {
							target_pos = received_motion_cmd.payload; // 'payload' is absolute position
							}
						else {
							// CMD_MOVE_RELATIVE
							target_pos = motor_states[motor_id].current_position + received_motion_cmd.payload;
							}

						// MotionPlanner_CalculateNewTarget sets direction and steps_to_go in motor_states[motor_id]
						int32_t steps_to_go = MotionPlanner_CalculateNewTarget(&motor_states[motor_id], target_pos);
						if (steps_to_go == 0) {
							// No movement required
							continue;
							}

						// --- Apply Motion Parameters and Start Motor ---
						// Set motor direction using the new MotionDriver API
						MotionDriver_SetDirection(motor_id, (motor_states[motor_id].direction == 1)); // direction 1=forward, 0=reverse

						// Set motor as active
						g_motor_active[motor_id] = true;

						// Start PWM generation for the motor using the new MotionDriver API
						// For now, we use the max speed. Acceleration profile will be handled in Этап 4.
						MotionDriver_StartMotor(motor_id, motor_states[motor_id].max_speed_steps_per_sec);

						// IMPORTANT: In this new asynchronous PWM architecture, the task does NOT block
						// waiting for the motor to complete a fixed number of steps.
						// The PWM will run continuously at the specified frequency until stopped.
						// The mechanism for decrementing steps_to_go and stopping the motor after
						// the required number of steps will be implemented in Этап 4 (Motion Planner Adaptation),
						// likely involving a timer interrupt or a separate mechanism to count pulses.
						break;
						}

					case CMD_STOP:
						{
							// Stop the specified motor
							MotionDriver_StopMotor(motor_id);
							g_motor_active[motor_id] = false;
							motor_states[motor_id].steps_to_go = 0; // Reset steps to go
							motor_states[motor_id].current_speed_steps_per_sec = 0; // Reset current speed
							break;
							}

					case CMD_SET_SPEED:
						{
							// Update max speed for the motor (future use by motion planner)
							motor_states[motor_id].max_speed_steps_per_sec = received_motion_cmd.payload;
							// If motor is currently running, need to update its frequency
							if (g_motor_active[motor_id]) {
								MotionDriver_StartMotor(motor_id, motor_states[motor_id].max_speed_steps_per_sec); // Restart with new speed
								}
							break;
							}

					case CMD_SET_ACCELERATION:
						{
							// Update acceleration for the motor (future use by motion planner)
							motor_states[motor_id].acceleration_steps_per_sec2 = received_motion_cmd.payload;
							break;
							}

					// Other commands (GET_STATUS, SET_CURRENT, ENABLE_MOTOR, PERFORMER_ID_SET)
					// are handled by Task_TMC_Manager or Task_Command_Parser, not here directly.

					default:
						break;
						}
			}
	osDelay(1); // Small delay to prevent busy-waiting if queue is empty or processing is very fast
	}
}
