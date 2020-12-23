# Excercises

1. Use `Button:E` to change between the excercises [3 sreens for 3 exercises (2,3,4), begin with exercise 2]
2. Use `Button:Q` to quit.
## Excercise 2 
1. Use `Button:A`, `Button:B`, `Button:C`, `Button:D`  to count the number of times they were pressed.
2. Click `left-mouse` to reset those numbers.
3. Move the `mouse` to change the screen in the direction of the `mouse`.

## Exercise 3
1. General Questions:
   - Tickless kernel, dynamic ticks or NO_HZ is a config option that enables a kernel to run without a regular timer tick.
   - The kernel tick is a timer interrupt that is usually generated HZ times per second.
2.  Use `Button:T` or `Button:U` to display the numbers that these buttons were pressed on the screen (these numbers are reset after every 15sec).
3.  Use `Button:S` to stop or continue the counter (increasing by one every second).

## Exercise 4
Show the output of all 15 ticks (outputs of 4 tasks with different priorities).
1. The priorities in the order: Task1< Task2<Task3<Task4: When 4 tasks are ready to run the orde would be: 4-2-3-1.
2. The priorities in the order: Task1> Task2>Task3>Task4 : When 4 tasks are ready to run the orde would be: 1-2-3-4.
3. The Tasks have the same priorities: When 4 tasks are ready to run the orde would be: 4-2-1-3. Task 1 go earlier than task 3 because of the delay of binary semaphore.
