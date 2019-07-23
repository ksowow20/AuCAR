/*
 * stateMachine.c
 *
 *  Created on: Jun 23, 2019
 *      Author: colson
 */

#include "stateMachine.h"
#include "AuCAR_conf.h"
#include "conf.h"



extern COUNTERS g_counters;


/*
 * machine functions (basic function of this class)
 * */
void StateMachine::machine_init(int index)
{
	info[index].state = 0;
	info[index].cmd1 = 0;
	info[index].cmd2 = 0;
	info[index].length = 0;
	if(info[index].data != NULL)
		free(info[index].data);
	else
		info[index].data = NULL;

	init_task_queue(index);
}
void StateMachine::run(void) {
	int nsize = 0;

	for(int i = 0 ; i < MACHINE_MAX_SIZE; i++)
	{
		nsize = get_vector_size(i);
		for (int j = 0 ; j < nsize; j++)
			machine(i, info[i].rdata.at(j));
		data_clear(i);
	}

	stateMachineTask_ST task;
	for(int i = 0; i < MACHINE_MAX_SIZE; i++)
	{
		while(1)
		{
			if(!get_task(i, &task))
				break;
			else
			{
				uint32_t getTick = HAL_GetTick();
				_DEBUG("NUM %d, Task Dequeue 0x%2X, 0x%2X, %d, %d, %d \r\n",i,task.cmd1,task.cmd2,task.length,task.data, getTick);

				free(task.data);
			}
		}
	}




	/* TODO get task */
	/* TODO set task */
//	stateMachineTask_ST task;
//	//get_task(0, &task);
//	while(1)
//	{
//		if(!get_task(0, &task))
//			break;
//		else
//		{
//			/* TODO task run */
//			uint32_t getTick = HAL_GetTick();
//			_DEBUG("Task Dequeue 0x%2X, 0x%2X, %d, %d, %d \r\n",task.cmd1,task.cmd2,task.length,task.data, getTick);
//
//			free(task.data);
//		}
//
//	}

//	free(task.data);

//	get_task(0, &task);
//
//	free(task.data);
//	get_task(1, &task);
//
//	free(task.data);
//	get_task(2, &task);
//
//	free(task.data);

}

void StateMachine::machine(int index, uint8_t data)
{
	stateMachine_ST *str;
	str = &this->info[index];

	if(str->state == 0x00)
	{
		str->count = 0;
		str->checksum = 0;
		if(data == 0xFF)
			str->state = 0x01;
		else
			str->state = 0x00;
	}
	else if(str->state == 0x01)
	{
		if(data == 0xFF)
			str->state = 0x02;
		else
			str->state = 0x00;
	}
	else if(str->state == 0x02)
	{
		str->cmd1 = data;
		str->state = 0x03;
	}
	else if(str->state == 0x03)
	{
		str->cmd1 |= (uint16_t)data << 8;
		str->state = 0x04;
	}
	else if(str->state == 0x04)
	{
		str->cmd2 = data;
		str->state = 0x05;
	}
	else if(str->state == 0x05)
	{
		str->cmd2 |= (uint16_t)data << 8;
		str->state = 0x06;
	}
	else if(str->state == 0x06)
	{
		str->length = data;
		str->state = 0x07;
	}
	else if(str->state == 0x07)
	{
		str->length |= (uint16_t)data << 8;
		str->state = 0x08;
		str->data = (uint8_t*)malloc(sizeof(uint8_t)*str->length);
	}
	else if(str->state == 0x08)
	{
		str->data[str->count++] = data;
		str->checksum += data;
		if(str->count == str->length)
		{
			str->state = 0x09;
		}
	}
	else if(str->state == 0x09)
	{
		//checksum//
		if(str->checksum == data)
		{
			//correct//
			g_counters.stateMachineCpltCounter[index]++;

		}
		//add task//
		add_task(index);

		free(str->data);
		//end task//
		str->checksum = 0x00;
		str->state = 0x00;
	}
}




/*
 * task management
 * */

void StateMachine::add_task(int index)
{
	stateMachineTask_ST task;
	//TODO
	//is queue empty?
	//and then add task
	if(!is_task_full(index))
	{
		task.cmd1 = this->info[index].cmd1;
		task.cmd2 = this->info[index].cmd2;
		task.length = this->info[index].length;
		task.data = (uint8_t*)malloc(sizeof(uint8_t)*task.length);
		for(int i = 0; i < task.length; i++)
			task.data[i] = info[index].data[i];
		task_enqueue(index, task);
		uint32_t getTick = HAL_GetTick();
		_DEBUG("NUM %d, Task Enqueue 0x%2X, 0x%2X, %d, %d, %d\r\n",index, task.cmd1, task.cmd2, task.length, task.data, getTick);
	}
	else
	{
		_DEBUG("Task is full \r\n");
	}
}
BOOL StateMachine::get_task(int index, stateMachineTask_ST *task)
{
	return task_dequeue(index, task);
}

/*
 * queue controls
 * */

void StateMachine::init_task_queue(int index)
{
	info[index].qfront = 0;
	info[index].qrear = 0;
	info[index].qcount = 0;
	info[index].qmax_count = 0;
}
BOOL StateMachine::is_task_empty(int index)
{
	return info[index].qfront == info[index].qrear ? true : false;
}
BOOL StateMachine::is_task_full(int index)
{
	return (info[index].qrear + 1) % TASK_MAX_QUEUE_SIZE == info[index].qfront ? true : false;
}
BOOL StateMachine::task_enqueue(int index, stateMachineTask_ST value)
{
	if(!is_task_full(index))
	{
		int preIndex = info[index].qrear;
		info[index].qrear = (info[index].qrear + 1) % TASK_MAX_QUEUE_SIZE;
		info[index].queue[preIndex] = value;
		info[index].qcount++;
		if(info[index].qcount > info[index].qmax_count)
			info[index].qmax_count = info[index].qcount;
		return true;
	}
	return false;
}
BOOL StateMachine::task_dequeue(int index, stateMachineTask_ST *value)
{
	if (!&info[index] || !value)
		return false;
	if (!is_task_empty(index))
	{
		/* initial setting */
		int preIndex = info[index].qfront;
		info[index].qfront = (info[index].qfront + 1) % TASK_MAX_QUEUE_SIZE;
		info[index].qcount--;
		/* new allocation */
		//*value = info[index].queue[preIndex];
		value->cmd1 = info[index].queue[preIndex].cmd1;
		value->cmd2 = info[index].queue[preIndex].cmd2;
		value->length = info[index].queue[preIndex].length;
		value->data = (uint8_t*)malloc(sizeof(uint8_t)*value->length);
		for(int i = 0 ; i < value->length ; i++)
			value->data[i] = info[index].queue[preIndex].data[i];
		/* free queue memory allocation */
		free(info[index].queue[preIndex].data);
		return true;
	}
	return false;
}
