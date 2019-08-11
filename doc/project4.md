       	 	     +-------------------------+
		     | CS 140                  |
		     | PROJECT 4: FILE SYSTEMS |
		     | DESIGN DOCUMENT         |
		     +-------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Tatia Tibunashvili <ttibu13@freeuni.edu.ge>
Salome Kasradze    <skasr13@freeuni.edu.ge>
Niko Barateli  	   <nbara14@freeuni.edu.ge>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

		     INDEXED AND EXTENSIBLE FILES
		     ============================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

filesys/inode.c
#define DIRECT_NUMB 120
#define BLOCK_PER_SECTOR BLOCK_SECTOR_SIZE / sizeof(block_sector_t)   "ერთი ბლოკი არის 512 ბაიტი, მისამართი 4 ბაიტი, შესაბამისად ერთ ბლოკში მხოლოდ 128 ცალი მისამართი ეტევა"
#define BLOCKS_DBL_INDIRECT BLOCK_PER_SECTOR * BLOCK_PER_SECTOR

struct inode_disk
  {
    block_sector_t direct_blocks[DIRECT_NUMB];		ფაილის Direct მისამართები.
    size_t used_direct;					გამოყენებული direct მისამართების რაოდენობა.
    block_sector_t indirect;				მისამართი indirect ების ბლოკზე, რომელშიც შენახული მისამართები ფაილის content ზე.
    size_t used_indirect;				გამოყენებული indirect მისამართების რაოდენობა.
    block_sector_t double_indirect;			მისამართი double_indirect ბლოკზე.
    size_t used_double_indirect;			გამოყენებული double_indirect ების რაოდენობა.
    int type;                           		1 for file, 0 for dir   (int რომ ზუსტად 512 ბაიტი იყოს inode_disk ის ზომა)
    off_t length;                       		ფაილის სიგრძე.
    unsigned magic;                     		/* Magic number. */
  };
 
struct inode
{
  ....
  struct lock lock;                                    ლოქი თითოეული ნოუდისთვის.
};
			
struct lock open_nodes_lock; 			        ლოქი გახსნილი აინოუდების ლისტში ჩამატება და წაშლის დაცვისთვის.

>> A2: What is the maximum size of a file supported by your inode
>> structure?  Show your work.
	inode_disk ში არის 120 direct, 1 indirect და 1 double_indirect მისამართი, შესაბამისად ფაილის ზომა რომელიც ამ მისამართებს შეუძლია დაფაროს( იმის გათვალისწინებით რომ
ერთი ბლოკი არის 512 ბაიტი) არის =>  120*512 + (128*512) + (128*128*512) = 8,515,584 = 8.1 MB 
8.5 MB მეტია ვიდრე pintOS ის დისკზე ფაილური სისტემის partition ის ზომა, მაგრამ რატომ აქვს ასეთი განაწილება მისამართებს ახსნილია ქვემოთ.

---- SYNCHRONIZATION ----

>> A3: Explain how your code avoids a race if two processes attempt to
>> extend a file at the same time.
  თითოეულ ნოუდს აქვს თავისი ლოქი, თუ კი პროცესს ფაილში ჩაწერა ან წაკითხვა უნდა , ილოქება ამ ფაილის შესაბამისი აინოუდის ლოქი ანუ ერთ ნოუდთან დროის ერთ მომენტში მხოლოდ ერთი პროცესი მუშაობს. რადგან ფაილში ჩაწერა მოიცავს ფაილის გაზრდასაც (თუ კი საკმარისი მეხსიერება არ ჰქონდა ფაილს), როდესაც ერთი პროცესი ზრდის ფაილს, მეორე
მას ვერ გაზრდის, რადგან ამ ფაილის ლოქზე დავეითდება და როდესაც ჩაწერას შეეცდება, მას უკვე გაზრდილი ფაილი დახვდება , თუ მაინც არ იყო საკმარისი ფაილის ზომა, მაშინ მეორე პროცესის გაზრდის.

>> A4: Suppose processes A and B both have file F open, both
>> positioned at end-of-file.  If A reads and B writes F at the same
>> time, A may read all, part, or none of what B writes.  However, A
>> may not read data other than what B writes, e.g. if B writes
>> nonzero data, A is not allowed to see all zeros.  Explain how your
>> code avoids this race.
 თუ კი B წერს რომელიმე, ფაილში მაშიმ ამ ფაილის აინოდუის ლოქი არის დალოქილი, შესაბამისად, რომელიმე სხვა ნაკადი ამ ფაილის ინფორმაციას ვერ წაიკითხავს და მხოლოდ B ს ჩაწერის შემდეგ ექნება საშუალება ფაილთან წვდომის.
 

>> A5: Explain how your synchronization design provides "fairness".
>> File access is "fair" if readers cannot indefinitely block writers
>> or vice versa.  That is, many processes reading from a file cannot
>> prevent forever another process from writing the file, and many
>> processes writing to a file cannot prevent another process forever
>> from reading the file.
 სიქრონიზაციის ეს ტიპი, რომელიც იმპელემტირებულია fairness არ არის, შესაბამისად, არ არის საშუალება ორმა პროცესმა ერთდროულად იკითხოს ერთი და იგივე ფაილის, რაც არასწორია. "fairness" ისთვის საჭიროა reader/writer ლოქები , pintos ის საწყის სინქორნიზაციის ტიპებში ასეთი ლოქი არ იყო და ჩვენ არ დავგვიმატებია.

---- RATIONALE ----

>> A6: Is your inode structure a multilevel index?  If so, why did you
>> choose this particular combination of direct, indirect, and doubly
>> indirect blocks?  If not, why did you choose an alternative inode
>> structure, and what advantages and disadvantages does your
>> structure have, compared to a multilevel index?
	პირობაში მოცემული, რომ PINTOS ის file system partition არ იქნება მეტი ვიდრე 8MB. იმისათვის, რომ ეს 8 MB სრულად დაიფაროს მისამართებით, საკმარისია 10 direct,
1 indirect და 1 double-indirect მისამართი, რადგან ერთი ბლოკი არის 512 ბაიტი -> 10*512 + 128 * 512 + (128*128 *512) = 8,459,264 B = 8 MB.
ასეთი მიდგმოით inode_disk ში, რჩებოდა 440 ბაიტი unused მეხსირება 512 დან და მივიღეთ გადაწყვეტილება, რომ მეხსიერება ბოლომდე გამოგვეყენებინა  direct მისამართების ხარჯზე.
ასეთი მიდგომით ფაილის ზომა სცდება თეორიულ ზღვარს, მაგრამ პირველ ბევრ ბაიტზე მიმართვა ფაილზე უფრო სწრაფი და direct იქნება ვიდრე წინა შემთხვევაში და double_indirect ების შესაბამისი ბლოკის გამოყოფა და ფაილის მეხსიერებაზე მიმართვა, მხოლოდ მაშინ იქნება საჭირო თუ ფაილი ძალიან დიდია. სხვა უმეტეს შემთხვევაში, direct მისამართები საკმარისი იქნება. თუ კი inode-disk ში საჭირო გახდა ზედმეტი ველის ჩამატება, კოდის შეცვლა მოხდება მხოლოდ DIRECT_NUMB ის შეცვლით, რაც გაანთავისუფლებს ადგილს სხვა ცვლადისთვის.

			    SUBDIRECTORIES
			    ==============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
thread.h
struct thread
{
	....
	struct dir * cwd;                   /*current working directory*/
	.....
}

---- ALGORITHMS ----

>> B2: Describe your code for traversing a user-specified path.  How
>> do traversals of absolute and relative paths differ?
	ორივე შემთხვევაში, როდესაც მაგალითად უნდა გაიხსნას ფაილი ან დირექტორია და გადმოგვეცემა absolute ან relative paths. path იპარსება "/ " დელიმიტერით და მოიძებნება
 ამ კონკრეტული ფაილის ან დირექტორიის მშობელი დირექტორია. თუ კი მოსაძებნი იყო  root , root ის მშობელი არის ისევ root.  მშობელი დირექტორიის მოძებნის შემდეგ, ხდება კონკრეტულად იმ ფაილის ან დირექტორიის node ის დაბრუნება რომელიც გადმოგვეცა. თუ კი path  იწყება / ით, მაშინ ძებნა იწყება root იდან, თუ / ის გარეშეა , ძებნა იწყება process ის
მიმდინარე working დირექტორიიდან.

---- SYNCHRONIZATION ----

>> B4: How do you prevent races on directory entries?  For example,
>> only one of two simultaneous attempts to remove a single file
>> should succeed, as should only one of two simultaneous attempts to
>> create a file with the same name, and so on.
  როდესაც ფაილი ემატება დირექტორიაში ან დირექტორიდაან იშლება, ან დირექტორიაში ხდება ძებნა, რადგანაც დირექტორია როგორც ფაილი ისე აღიქმება და შესაბამისად მასაც აქვს აინოდი, აინოუდს კი აქვს ლოქი, ილოქება ამ დირექტორიის შესაბამისი აინოდი. შესაბამისად, როდესაც ორი პროცესი ცდილობს ფაილის წაშლას, მხოლოდ ერთი მოახერხებს და იგივე ფაილში ჩამატებაზეც.


>> B5: Does your implementation allow a directory to be removed if it
>> is open by a process or if it is in use as a process's current
>> working directory?  If so, what happens to that process's future
>> file system operations?  If not, how do you prevent it?
	დირექტორიის remove არ მოხდება თუ  კი ეს დირექტორია ცარიელი არ არის. თუ კი ცარიელია, ნებისმიერ შემთხვევაში ფაილის remove ია საჭირო თუ დირექტორიის, ხდება მხოლოდ
მისი შესაბამისი inode ის თვის removed პარამეტრის დასეტვა. მანამ სანამ მისი შესაბამისი აინოუდი რეალურად წაიშლება დისკიდან, თუ კი ეს დირექტორია იყო რომელი პროცესის 
მიმდინარე დირექტორია- მასზე მიმართვების დროს ხდება შემოწმება removed არის თუ არა, და იგივე ეფექტს იძლევა, როგორც რეალურად წაშლა.

---- RATIONALE ----

>> B6: Explain why you chose to represent the current directory of a
>> process the way you did.
	thread სტრუქტურა ინახავს მიმდინარე working directory ზე მისამართს. პროცესის შექმნისას, მშობელი პროცესის მიმდინარე დირექტორია ხდება შვილი პროცესის დირექტორია. თუ
კი პროცესი არის initial_process იგივე main, მაშინ მისი მიმდინარე დირექტორია არის root, და ყველა მისი შვილობილი პროცესი დირექტორიის შესაცვლელად იქნებს syscall 
chdir. ასეთი ფორმით განსაზღვრა მიმდინარე დირექტორიის მარტივი იყო ლოგიკურადაც და იმპელემნტაციისთვისაც .


			     BUFFER CACHE
			     ============

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

static struct bitmap * bitmap;				 ბიტმეპი, რომელიც ასახავს ქეშის რომელი ინდექსებია თავისუფალი.
static struct cache_block cache[CACHE_SIZE];		 cache_block ების მასივი ანუ ქეში.
static struct list read_ahead_sectors;			 list რომელშიც იყრება ის სექტორები რომლების წაკითხვა read_ahead ნაკადის მიერ ასინქრონულად უნდა მოხდეს.
struct semaphore next_sector;				 რამდენი სექტორის წაკითხვა უნდა მოხდეს წინასწარ სანამ, წამკითხველი პროცესი დავეთიდება.
struct lock list_lock;					 ლოქი, რომ ლისტში ჩაინსერტება და ამოღება ატომურად მოხდეს.


filesys/cache.h
#define CACHE_SIZE 64				
struct cache_block
{
	block_sector_t sector_idx;			დისკზე სექტორის მისამართი, რომელსაც ქეშის კონკრეტული ბლოკი აღწერს.
	void * data;					ფაილის content , რომელიც ქეშშია.
	struct lock sector_lock;			ქეშის i-ური ბლოკის ლოქი.
	bool dirty;					მოხდა თუ არა, ქეშში არსებულ მეხსირებაში ჩაწერა.
};	

---- ALGORITHMS ----

>> C2: Describe how your cache replacement algorithm chooses a cache
>> block to evict.
 ქეშის ყოველი ბლოკი ინახავს ინფორმაციას მოხდა თუ არა, მის შესაბამის დატა ში ცვლილებების შეტანა, (write). eviction ის დროს, პრიორიტეტი ენიჭება ისეთ სექტორებს რომელსაც 
dirty ბიტი არ აქვთ, ანუ რომლის უკან დისკზე გადაწერაც საჭირო არ არის. თუ კი ასეთი არცერთი ბლოკი არ იყო, მაშინ რენდომად რომელიღაც გადაიწერება დისკზე და მისი შესაბამისი ინდექსი ქეშში თავისუფლად გამოცხადდება. 

>> C3: Describe your implementation of write-behind.
 როგორც პირობაში, იყო მოცემული cache ის დისკზე გადაწერა შეგვეძლო გამოგვეძახებინა timer.c ში არსებული timer_sleep ფუნქციაში, ანუ timer ის გარკვეული რაოდენობის ტიკების შემდეგ  ხდება cache ში არსებული დატის ისევ დისკზე გადაწერა. 

>> C4: Describe your implementation of read-ahead.
 cache ის ინიციალიზაციის დროს, ისტარტება ახალი thread ,რომელიც აკეთებს read_ahead. წასაკითხ სექტორებს იღებს read_ahead_sectors ლისტის საშუალებით. როდესაც რომელიმე სექტორი უნდა წაიკითხოს პროცესმა, მისი მომდევნო სექტორი ჩავარდება ლისტში და სემაფორა next_sector გაიზრდება 1 ით. პროცესი რომელიც ამ სექტორების წაკითხვაზეა
პასუხისმგებელი დავიეთებულია next_sector სემაფორაზე. როდესაც სემაფორა გაიზრდება , სექტორს, რომელიც ლისტში იყო ამოიღებს და წაიკითხავს, მეორე პროცესისგან დამოუკიდებლად. თუმცა ამ მომდევნო სექტორის cache ში ჩაწერისთვისაც იყენებს cache ის ლოქს, რომ ქეშის სინქრონიზაცია დაცული იყოს. 

---- SYNCHRONIZATION ----

>> C5: When one process is actively reading or writing data in a
>> buffer cache block, how are other processes prevented from evicting
>> that block?
  თითოეული ბლოკი დალოქილია თავის ლოქზე, სანამ მასზე ოპერციები სრულდება, შესაბამისად მხოლოდ ერთ პროცესს ექნება წვდომა ამ ბლოკზე. 

>> C6: During the eviction of a block from the cache, how are other
>> processes prevented from attempting to access the block?
 გარდა ბლოკებზე ლოქისა, ქეშშს აქვს cache_lock რომელიც გამოიყენება, cache_block ების მასივზე მიმართვის დროს, მაგალითად როდესაც სექტორის ნომრის მიხედვით უნდა მოვძებნოთ ჩანაწერი რომელიც ამ სექტორს შეესაბამება. როდესაც eviction უნდა მოხდეს, ილოქება cache, შესაბამისად მასში რომელიმე სექტორის ძებნას ვერავინ ვერ შეძლებს. როდესაც იპოვის ინდექსს რომელიც უნდა გაძევდეს ქეშიდან, cache_lock ის რილიზი ხდება, მაგრამ ილოქება ნაპოვნი ბლოკის ლოქი, რათა რომელიმე  სხვა პროცესმა არ ეცადოს გასაძევებელი ბლოკიდან ან წაკითხვა ან ჩაწერა.
	


			   SURVEY QUESTIONS
			   ================

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

>> In your opinion, was this assignment, or any one of the three problems
>> in it, too easy or too hard?  Did it take too long or too little time?

>> Did you find that working on a particular part of the assignment gave
>> you greater insight into some aspect of OS design?

>> Is there some particular fact or hint we should give students in
>> future quarters to help them solve the problems?  Conversely, did you
>> find any of our guidance to be misleading?

>> Do you have any suggestions for the TAs to more effectively assist
>> students in future quarters?

>> Any other comments?

