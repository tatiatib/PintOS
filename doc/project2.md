Design Document for Project 2: User Programs
============================================

## Group Members

* Tatia Tibunashvili <ttibu13@freeuni.edu.ge>
* Salome Kasradze <skasr13@freeuni.edu.ge>
* Niko Barateli <nbara14@freeuni.edu.ge>



ARGUMENT PASSING
================

	---- DATA STRUCTURES ----

	>> A1: Copy here the declaration of each new or changed `struct' or
	>> `struct' member, global or static variable, `typedef', or
	>> enumeration.  Identify the purpose of each in 25 words or less.

		დამატებითი ცვლადები ან სტრუქტურები არ გამოგვიყენებია.
	---- ALGORITHMS ----

	>> A2: Briefly describe how you implemented argument parsing.  How do
	>> you arrange for the elements of argv[] to be in the right order?
	>> How do you avoid overflowing the stack page?

		command line ის არგუმენტები სტეკში ლაგდება ზუსტად იმ თანმიმდევრობით, როგორც პირობის მაგალითში იყო მოცემული.
		ჯერ არგუმენტების მნიშვნელობები reverse თანმიმდევრობით, შემდეგ, ჩაწერილი მნიშნვნელობების ჯამური ზომა იზრდება ისე რომ, 4 ის ჯერადი იყოს. ამის შემდეგ, კი იწერება მისამართები(სტეკში მისამართები) ამ მნიშნველობებზე და ეს მისამართები მთავრდება ნულოვანი მისამართით. (რეალურად აქაც ჩაწერა პირიქით ხდება, იმისთვის რომ პირველივე არგუმენტზე მისამართი უფრო ახლოს ჰქონდეს სტეკში), შემდეგ იწერება მისამართი მანამდე ჩაწერილი მისამართების მასივზე,  არგუმენტების რაოდენობა და return address.  reverse თანმიმდევრობით ჩაწერისთვის თავიდანვე იმ მასივში რომელშიც ვინახავდით არგუმენტებს ცალ-ცალკე , ჩავწერეთ ზუსტად ისე როგორც სტეკში ჩაიწერებოდა.  
	---- RATIONALE ----

	>> A3: Why does Pintos implement strtok_r() but not strtok()?
		strtok ინახავს სტატიკურ პოინტერს ტოკენზე, strtok_r ს კი ჩვენ გადავცემთ პოინტერს. strtok ის პოინტერი გასცემს ინფორმმაციას , რადგან სტატიკური ცვლადები წარმოადგნეს 	გლობალური სთეითის ნაწილს, შესაბამისად მრავალი პროცესის შემთხვევაში არასწორი იქნებოდა, რომელიმე პროცესის ცვლადი გლობალურად შეგვენახა. strtok_r არის strtok 	 ის reentrant ვარიანტი, რაც ამ პრობლემას არ იწვევს.

	>> A4: In Pintos, the kernel separates commands into a executable name
	>> and arguments.  In Unix-like systems, the shell does this
	>> separation.  Identify at least two advantages of the Unix approach.

SYSTEM CALLS
============

---- DATA STRUCTURES ----

	>> B1: Copy here the declaration of each new or changed `struct' or
	>> `struct' member, global or static variable, `typedef', or
	>> enumeration.  Identify the purpose of each in 25 words or less.

	thread.c
	thread struct:
	    struct file * fds[MAX_FILES];              	file descriptors array 
	    size_t last_fd;				ბოლო თავისუფალი ინდექსი fd ში
	    char * elf;					ნაკადის executable ფაილის სახელი
	    struct list children;			Child სტრუქტურების ლისტი, Child სტრუქტურა აღწერ thread როგორც სხვა ნაკადის შვილს
	    struct child * me;				თავის Child სტრუქტურაზე მისამართი, ანუ თვითონ არის წარმოდგენილი როგორც 								შვილი სხვა პროცესის და გამოიყენება მშობელთან კომუნიკაციისთვის.

	struct child{
	  int return_value;				მნიშვნელობა, რომლითაც შვილი exit დება. საჭიროა რომ, თუ მშობელი ელოდება მიიღოს ინფორმაცია შვილის return_value ს 								შესახებ.
	  int error_id;					იმ შემთხვევაში, თუ შვილმა ვერ მოახერხე elf ფაილის გახსნა და შესრულება ანუ თუ შვილი პროცესი ვერ დაისტარტა, მშობლემა	 							უნდა მიიღოს ინფორმაცია.
	  tid_t tid;					შვილი პროცესის id.
	  struct semaphore sema;			სემაფორა გამოიყენება exec  და wait ის სწორად შესრულებისთვის. მშობელი რომელიც ელოდება რომელიმე შვილს, ცდილობს 								მისი შესაბამისი child sema ს  შემცირებას, შვილი კი როცა მორჩება შესრულებას, გაზრდის ამ სემაფორას.
	  struct semaphore exec;			გამოიყენება იმისთვის, რომ მშობელი დაელოდოს შვილს სანამ ის დაისტარტება. და მხოლოდ მას შემდეგ დააბრუნოს შვილის ID. 							თუ შვილის სტარტში მოხდა შეცდომა მშობელმა უნდა მიიღოს ინფორმაცია და დააბრუნოს error_id. სანამ 
	  struct list_elem child_elem;			list element, იმისთვის რომ სტრუქტურა ჩაემატოს მშობლის შვილების ლისტში.
	};
	
	syscall.c
	static syscall functions[FUNC_NUMB]     	ფუნქციების მისამართების array, ფუნქციები დალაგებულია სისქოლების ნომრების მიხედვით.

	>> B2: Describe how file descriptors are associated with open files.
	>> Are file descriptors unique within the entire OS or just within a
	>> single process?
		თითოეულ ნაკადს აქვ ლისტი თავისი file desctiptor სთვის და ყოველ შემდეგ გახსნილ ფაილს დესქრიპტორი ენიჭება ინკრემენტულად.
		ლისტში  i-ურ დესკრიპტორზე შესაბამისი ფაილია, მენოლე და პირველ ელემენტები ცარიელია, რადგან წარმოადგენს STDIN STDOUT ს, როგორც 0, და 1

---- ALGORITHMS ----

	>> B3: Describe your code for reading and writing user data from the
	>> kernel.
		static syscall functions[FUNC_NUMB]  არის მასივი სადაც შენახული თითოეული სისქოლის handler, და ამ ფუნქციის პოზიცია მასივში იგივეა რაც მისი syscall number ის პოზიცია. შესაბამისად როდესა საჭიროა რომელიმე სისქოლის დამუშავება, გამოიძახებ შესაბამისი handler, თითოეული hanlder კი არის ტიპის -> typedef void (*syscall)(void *args, uint32_t * eax); პირველი არგუმენტი არის მისამართი არგუმენტებზე, მეორე რეგისტრი  სადაც უნდა ჩაწეროს დასაბრუნებელი მნიშვნელობა. გადმოცემული არგუმენტების და მისამართების ვალიდურობა მოწმდება ფუნქციით static bool check_boundaries (void * args), რომელიც ამოწმებს რომ მისამართი იყოს user space ში და შეესაბამებოდეს ვალიდური ფიზიკური მისამართი.
handler ში ეს არგუმენტები syscall ის შინაარსის მიხედვით გამოიყენება. თუ რომელიმე არგუმენტმა ვერ გაიარა შემოწმება მისამართების ვალიდურობაზე, პროცესი მაშინვე დაექსითდება -1 სტატუსით. სხვა დანარჩენი return_value ები იწერება eax რეგისტრში.
	>> B4: Suppose a system call causes a full page (4,096 bytes) of data
	>> to be copied from user space into the kernel.  What is the least
	>> and the greatest possible number of inspections of the page table
	>> (e.g. calls to pagedir_get_page()) that might result?  What about
	>> for a system call that only copies 2 bytes of data?  Is there room
	>> for improvement in these numbers, and how much?
	სანამ დატა ჩაკოპირდება, მანამდე ხდება შემოწმება pagedir_get_page() მეთოდით. შესაბამისად თუ დატის პოინტერი უკვე დამეპილია და ის user space შია ანუ ვალიდური მისამართია ჩაწერა მოხდება. ანუ ყოველი არგუმეტისთვის ეს შემოწმებები იგივეა. 
	>> B5: Briefly describe your implementation of the "wait" system call
	>> and how it interacts with process termination.
	
	wait იძახებს process_wait იმ pid ზე რომელიც wait ში არგუმენტად გადაეცა. process_wait კი ეძებს თავის შვილებში იმ შვილის შესაბამის ნოუდს რომლის აიდი არის გადმოცემული აიდი და შვილის შესაბამის სემაფორაზე ვეითდება. როცა სემაფორის ინკრემენტი მოხდება, ანუ როდესაც შვილმა მუშაობა დაამთავრა, შვილის ჩანაწერი იშლება მშობლის შვილების ლისტიდან. ყოველთვის როცა შვილი მუშაობას ამთავრებს, იძულებით (force_exit) ან exception ით, სემაფორას ზრდის, რადგან თუ მშობელი ელოდება, მიეცეს მშობელს საშუალება შეიტყოს თუნდაც არასასრუველი შედეგით დასრულების შესახებ.

	>> B6: Any access to user program memory at a user-specified address
	>> can fail due to a bad pointer value.  Such accesses must cause the
	>> process to be terminated.  System calls are fraught with such
	>> accesses, e.g. a "write" system call requires reading the system
	>> call number from the user stack, then each of the call's three
	>> arguments, then an arbitrary amount of user memory, and any of
	>> these can fail at any point.  This poses a design and
	>> error-handling problem: how do you best avoid obscuring the primary
	>> function of code in a morass of error-handling?  Furthermore, when
	>> an error is detected, how do you ensure that all temporarily
	>> allocated resources (locks, buffers, etc.) are freed?  In a few
	>> paragraphs, describe the strategy or strategies you adopted for
	>> managing these issues.  Give an example.

	თუ კი არგუმენტის მისამართი არავალიდური იყო, ან იუზერ პროცესმა სცადა ისეთი მეხსირებაზე წვდომა რომელიც მას არ ეკუთვნოდა,  ორივე შემთვხევაში, პროცესი გამოიძახებს force_exit ფუნქციას, რომელიც უზრუნველყოფს იმას, რომ დარეთარნდეს -1 ით, და გამოიძახოს thread_exit(), რაც თავის მხრივ, გამოიძახებს process_exit().  ნებისმიერ შემთხვევაში  thread_exit() ში მოხდება ყველა გამოყენებული ინფორმაციის გასუფთავება, გახსნილი ფაილების დახურვა და შვილების child სტრუქტურების წაშლა. 

---- SYNCHRONIZATION ----

	>> B7: The "exec" system call returns -1 if loading the new executable
	>> fails, so it cannot return before the new executable has completed
	>> loading.  How does your code ensure this?  How is the load
	>> success/failure status passed back to the thread that calls "exec"?
	
	process_execute არ  დააბრუნებს შვილის აიდის სანამ, შვილობილი პროცესის load არ მოხდება. ეს სინქრონიზაცია გაკეთებულია child სტრუქტურაში არსებული სემაფორის საშუალებით. როდესაც, შვილობილი პროცესი დაასრულების executable ის ლოადს, ის სემაფორას გაზრდის, და მშობელი რომელიც process_execute ში, სწორეს ამ სემაფორაზე იყო დავეითებული, მიიღებს ინფორმაციას, წარმატებულობის ან წარუმატებლობის შესახებ.
---- RATIONALE ----

	>> B9: Why did you choose to implement access to user memory from the
	>> kernel in the way that you did?
		ყველა გადაწყვეტილება იყო problem ის  straightforward ამოხსნა გარვკეულწილად. იმისთვის რომ, მარტივი ყოფილიყო იმპლემენტაცია და შესაბამისად შეცდომის დაშვების ალბათობა ნაკლები. 

	>> B10: What advantages or disadvantages can you see to your design
	>> for file descriptors?
		პირობაში მოცემული იყო, რომ გახსნილი file descriptors ების რაოდენობა წესით არ უნდა შეგვეზღუდა, მაგრამ მაინც თუ საჭირო იქნებოდა, უნდა ყოფილიყო მინიმუმ 128.
	ეს ნაწილი გამოვიყენეთ და ფაილები შევინახეთ, წინასწარ გამზადებულ მასივში, სადაც ინდექსი წარმოადგენს descriptor ს. რა თქმა უნდა ზედა ზღვარი არ არის უპირატესობა. 
	იქიდან გამომდინარე, რომ პირობაში ასევე ეწერა რომ, filesys დირექტორიაში უმჯობესი იქნებოდა არცერთი ფაილი არ შეგვეცვალა, file სტრუქტურაში ლისტ ელემენტის 	დამატებისგან თავი შევიკავეთ და შესაბამისად სტრუქტურა რომელიც ლისტი უნდა ყოფილიყო გახდა მასივი ფაილების. 
	>> B11: The default tid_t to pid_t mapping is the identity mapping.
	>> If you changed it, what advantages are there to your approach?
	 არ შეგვიცვლია.

	
