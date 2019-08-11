        	+---------------------------+
		    | CS 140                    |
		    | PROJECT 3: VIRTUAL MEMORY	|
		    |	DESIGN DOCUMENT         |
		    +---------------------------+

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

			PAGE TABLE MANAGEMENT
			=====================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

thread.h 
struct thread {
	void * esp;      სტეკ-პოინტერის მიმდინარე მისამართი.
}

page.h
struct page
{
	uint8_t * upage;	           virtual page
	struct file_info * file_entry;	   ფაილის შესახებ ინფორმაცია, თუ page არის ან exec ფაილისთვის განკუთვნილი ან mmap სთვის.
	struct hash_elem hash_elem;       /* Hash table element. */
	int swap_block_index;		  swap block ის ინდექსი თუ page სვაპშია და თუ  არა -1.
};

struct file_info{
	struct file * file;		  file სრუქტურა რომლის შიგთავსი page ში უნდა ჩაიტვირთოს.
	off_t ofs;		          საწყისი ინდექსი, საიდანაც კითხვა უნდა დაიწყოს.	
  	uint32_t read_bytes;	          წასაკითხი ბაიტების რაოდინობა.
	uint32_t zero_bytes;              0 ების რაოდენობა რომ, page ბოლომდე შეივსოს.
	bool writable;			  writable არის თუ არა ფაილი.
	int mmapid;			  mmapid თუ კი ფაილი დამეპილია ამ ვირუტალურ პეიჯში. ემთხევა ფაილ დესკრიპტორს.	
};


frame.h
struct frame
{ 
	void * upage;                    ვირტუალური მისამართი
	void * kpage;                    ფიზიკური მისამართი.
	struct hash_elem hash_elem;     /* Hash table element. */
	struct list_elem elem;		list_elem, გამოიყენე frame ის ლისტში ჩამატებისთვის eviction ის დროს.	
	struct thread * owner;		ვირტუალური მისამართის მფლობელი პროცესი.
	bool pinned;			დაპინულია თუ არა ფრეიმი.
	bool writable;			შეიძლება თუ არა frame ში ჩაწერა.	
};

---- ALGORITHMS ----

>> A2: In a few paragraphs, describe your code for accessing the data
>> stored in the SPT about a given page.
როდესაც პროცესი აკეთებს მოთხოვნაზე pageზე, მასში არ იწერება ინფორმაცია( მაგალითად საწყის ეტაპზე, load_segmentში, სადაც exec file ის შიგთავსის ჩაწერა ხდება
pageში, პროცესი მხოლოდ ინახავს ვირტუალური ფეიჯის მისამართს  page tableში შემდგომი გამოყენებისთვის). მაშინ როდესაც პროცესი მიმართავს page, რომელიც მანამდე მოითხოვა, მოხდება page fault რის შემდეგაც მოწმდება fault_addr არის თუ არა ვალიდური, ანუ მისი შესაბამისი virtual page მოიძებნება თუ არა პროცესის hash table ში, და ვალიდური პასუხის შემთხვევაში,  ჯერ გამოიყოფა ფიზიკური page, ფიზიკურ pageში ჩაიწერება ინფორმაცია რომელიც პროცესმა მოითხოვა( მაგალითად ფაილის შიგთავსი) და ეს ფიზიკური page, pagedir ში გაფორმდება  როგორც პროცესის მიერ მოთხოვნილი ვირტუალური ფეიჯის შესაბამისი ფრეიმი.


>> A3: How does your code coordinate accessed and dirty bits between
>> kernel and user virtual addresses that alias a single frame, or
>> alternatively how do you avoid the issue?
accessed და dirty ბიტების შემოწმება ხდება მხოლოდ eviction ში. eviction კი ხდება frame_table ზე  და თითოეული struct frameს აქვს როგორც upage(ვირტუალური ფეიჯი) ასევე 
kpage(ფიზიკური მისამართი). accessed და dirty ბიტების შემოწმება ხდება pagedir_is_dirty (uint32_t *pd, const void *upage); 
pagedir_is_accessed (uint32_t *pd, const void *upage);
ფუნქციებით. მეორე არგუმენტი რაც ვირტუალური ფეიჯია
უკვე არსებობს struct_frameში, შესაბამისად  მარტივად ხდება შემოწმება. 

---- SYNCHRONIZATION ----

>> A4: When two user processes both need a new frame at the same time,
>> how are races avoided?

frame table ს ეკუთვნის  lock -> "struct lock alloc_lock;" სანამ ნებისმიერი პროცესი გამოიძახებს frame_alloc-ს რომელიც რეალურად გამოყოფს ფეიჯს, alloc_lock ილოქება , შესაბამისად
ფრეიმის გამოყოფაზე race condition არ ხდება.

---- RATIONALE ----

>> A5: Why did you choose the data structure(s) that you did for
>> representing virtual-to-physical mappings?
struct page ში ინახება მხოლოს virtual page, ეს იმიტომ რომ თითოეულ პროცესს აქვს თავისი hash list , რომელიც ასეთი ტიპის სტრუქტურებს ინახავს. პროცესისთვის არსებობს მხოლოდ
ვირტუალური  ფეიჯები. struct_frame კი ინახავს როგორც ვირტუალურ ასევე ფიზიკურ ფეიჯსაც ანუ სწორედ frame სტრუქტურაში ხდება ამ ორი ტიპის ფეიჯის mapping.
ეს იმისთვის, რომ გაგვემიჯნა აბსტრაქცია რომელსაც პროცესი ხედავს ვირტუალიზაციის სახით.  კერნელს კი აქვს წვდომა რეალურ frame ებთან. 


		       PAGING TO AND FROM DISK
		       =======================

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

ზემოთ აღწერილი სტრუქტურების გარდა, დამატებით სხვა სტრუქტურა გამოყენებული  არ არის.
---- ALGORITHMS ----

>> B2: When a frame is required but none is free, some frame must be
>> evicted.  Describe your code for choosing a frame to pagedir_is_dirty (uint32_t *pd, const void *upage);evict.

frame ების კლასიფიკაცია ხდება სამი ტიპის მიხედვით. 1. not_accessed  - not dirty. 2. not_dirty , 3. dirty.
frame ების hash_list ზე იტერაციით ხდება ფრეიმების შესაბამის ლისტში ჩაწერა.
უპირატესობა ენიჭება ლისტს რომელიც შეიცავს not_accessed  - not dirty ფრეიმებს, შემდეგ  not_dirty ლისტს და ბოლოს dirty ლისტს.
თუ not_accessed  - not dirty ის შესაბამისი ლისტის ზომა გარკვეულ ღვარზე მაღალია (ჩვენ შემთხვევაში 10). random ად ამ ლისტიდანა ამოირჩევა frame რომელიც გაძევდება
მეხსიერებიდა. ქვედა ზღვარი საჭიროა იმისთვის რომ ასეთი ტიპის ლისტში უფრო სავარაუდოა რომ, ნაკლები frame მოხვდეს ვიდრე მასზე ნაკლებად პრიორიტეტულ ლისტში და
შედეგად მოხდებოდა ისე, რომ ყოველთვის რაღაც მცირე სეტიდან მოხდებოდა არჩევა გასაძევებელი frame ების. თუ კი პირველმა ლისტმა მოთხოვნა ვერ დააკმაყოფილა , არჩევა ხდება
not_dirty ლისტიდან და ბოლოს თუ კი მეორე ლისტიც ზომას შეუსაბამო იყო აუცილებლად გაძევდება რანდომ frame dirty ლისტიდან.

>> B3: When a process P obtains a frame that was previously used by a
>> process Q, how do you adjust the page table (and any other data
>> structures) to reflect the frame Q no longer has?
პროცეს Q ს page_table ში არაფერი იცვლება, რადგან მისი page_table მხოლოდ ვირტუალურ მისამართს ინახავს, იმისდა მიუხედავად ამ ვირტუალური მისამართის უკან რეალური ფრეიმი დგას თუ არა. შეიცვლება frame_table. struct frame რომელიც Q პროცესის upage ს ინახავდა, ამოშლის Q პროცესის pagedir იდან upageს, palloc_free ს გაუკეთებს მის შესაბამის kpage ს
და სტრუქტურას წაშლის frame_table დან. kpage ის წაშლის შემდეგ კი ხელახლა palloc შეძლებს დააბრუნოს ახალი ფიზიკური ფრეიმი, მაგარამ ეს ფიზიკური ფრეიმი  P პროცესის pagedir ში
გაფორმდება მხოლოდ მაშინ, როდესაც ამ page ს რეალურად მიმართავს. 
>> B4: Explain your heuristic for deciding whether a page fault for an
>> invalid virtual address should cause the stack to be extended into
>> the page that faulted.

page_fault ის მოხდენის შემდეგ, თუ კი fault_addr არ იყო პროცესის page table ში, მაშინ მოწმდება არის თუ არა ეს სტეკის გაზრდის გამო გამოწვეული შეცდომა.
არის ორი ვარიანტი თუ შეცდომა მოხდა userspace ში და შესაბამისად შეგვიძლია შევამოწმოთ int_frame ის esp  ან თუ შეცდომა მოხდა kernelspace ში და უნდა შევამწმოთ, 
იუზერსფეისიდან კერნელსფეისში გადასვლის მომენტში( ანუ სისქოლის გამოძახების დროს) პროცესის esp, რაც ინახება thread სტრუქტურის ერთ-ერთ field ში.
  
შემოწმება კი ხდება სამი ტიპის განსხვავებაზე სტეკპოინტერსა და fault_addr შორის. თუ fault_addr 4 ბაიტით ჩამოცდა ან 32 ით ჩამოცდა ან fault_addr უფრო მაღალ მისამართზეა
ვიდრე მიმდინარე სტეკპოინტერი (როდესაც პროცესში აღიწერება დიდი მასივი სტეკში). ამ სამი პირობიდან რომელიმეს დაკმაყოფილების შემთხვევაში გამოიყოა ახალი frame სტეკისთვის.


---- SYNCHRONIZATION ----

>> B5: Explain the basics of your VM synchronization design.  In
>> particular, explain how it prevents deadlock.  (Refer to the
>> textbook for an explanation of the necessary conditions for
>> deadlock.)
   გამომდინარე იქიდან, რომ frame_table არის გლობალური, frame_table ზე ოპერაციები არის დაცული ერთი ლოქით და შესაბამისად სხვადასხვა პროცესების
მიერ ამ frame_table ზე მიმართვა სინქრონიზირებულია. SPT ყველა პროცესს აქვს თავისი და რადგან ერთი და იგივე ნაკადი ერთ მომენტში კოდის ორ სხვადასხვა ნაწილში ვერ იქნება
SPT დამატებით დაცვას არ საჭიროებს.
რადგანაც პროცესების shared data ,ერთი ლოქითაა დაცული, დედლოქის საშიშროება არ არსებობს. თუ კი ეს ლოქი რომელიმე პროცესმა აიღო და შემდეგ გარკვეული პრობლემებიის გამო
ფუნქციიდან დარეთარნება უნდა მოხდეს, მაქსიმალურად გათვალისწინებულია, რომ სანამ ნაკადი დარეთარნდება ეს ლოქი განთავისუფლდეს.

>> B6: A page fault in process P can cause another process Q's frame
>> to be evicted.  How do you ensure that Q cannot access or modify
>> the page during the eviction process?  How do you avoid a race
>> between P evicting Q's frame and Q faulting the page back in?
  
  evictionის process ის დროს, ანუ მაშინ როდესაც უნდა ავარჩიოთ რომელი ფრეიმი გადავა სვაპში ან წაიშლება, ყველა პროცესს ჯერ კიდევ ის ფრეიმები აქვს რაც ევიქშენის დაწყებამდე ჰქონდა.  Q პროცესს პრობლემა შეექმნება მხოლოდ მაშინ რაც მისი pagedir იდან წაიშლება იმ ფრეიმის ვირტუალური მისამართი, რომელიც სვაპში უნდა გადავიდეს. პრობლემის გამოწვევა
რეალურად page_fault ია, მას შემდეგ რაც, page_fault მოხდება Q პროცესიც შეეცდება  ფრეიმის ალოექეიშენს, მაგრამ რადგან frame_table  დალოქილია, უნდა დაელოდოს, როდის გაძევდება ბოლომდე მისი ფეიჯი მეხსიერებიდან და მხოლოდ მას შემდეგ ხელახლა შემოიტანოს ან სვაპიდან ან დისკიდან თავისი ფეიჯი.
>> B7: Suppose a page fault in process P causes a page to be read from
>> the file system or swap.  How do you ensure that a second process Q
>> cannot interfere by e.g. attempting to evict the frame while it is
>> still being read in?
  სანამ P პროცესი, რეალურ დატას არ ჩაწერს ფეიჯში, ეს ფეიჯი დაპინულია, რაც გულისხმობს იმას რომ eviction ის ალგორითმში საერთოდ არ მონაწილეობს, მხოლოდ მას შემდეგ
რაც წაიკითხავს და ჩაწერს თავის pagedir ში ანუ დააფიქსირებს ამ ფეიჯის არსებობას, მოხდება ფეიჯის ანპინი და ამ  მომენტში უკვე მისი გაძევებაც შესაძლებელია მეხსიერებიდან.

>> B8: Explain how you handle access to paged-out pages that occur
>> during system calls.  Do you use page faults to bring in pages (as
>> in user programs), or do you have a mechanism for "locking" frames
>> into physical memory, or do you use some other design?  How do you
>> gracefully handle attempted accesses to invalid virtual addresses?
  სისტემ ქოლების დროს  virtual_addresse ების შემოწმება ზუსტად ისე ხდება როგორც paga_fault ის დროს, ანუ მოწმდება რამდენად არის მისამართი SPT ში და თუ ვალიდური მისამართია, 
ამ ფეიჯის მიერ მოთხოვნილი ინფორმაცია(ანუ რაც მასში უნდა ეწეროს) ჩაიწერება რეალურად . თუ კი არავალიდური მისამართი იყო ან ნალი იყო ან შემოწმების რომელი პირობა ვერ დააკმაყოფილა, პროცესი მოკვდება -1 სტატუსით.
 
---- RATIONALE ----

>> B9: A single lock for the whole VM system would make
>> synchronization easy, but limit parallelism.  On the other hand,
>> using many locks complicates synchronization and raises the
>> possibility for deadlock but allows for high parallelism.  Explain
>> where your design falls along this continuum and why you chose to
>> design it this way.
 თავიდანვე პრინციპი იყო ის, რომ უნდა დაილოქოს კოდის ის ნაწილი რომელზეც race condition შეიძლება რომ მოხდეს, ანუ ის დატა რომელიც პროცესებს საერთო აქვთ.
 ასეთ დატას ამ დავალებაში წარმოადგენს frame_table და სვაპი, რადგანაც გლობალურია ყველა პროცესისთვის. კოდის ის ნაწილი რომელიც ლოქშია ჩასმული მაქსიმალურად შეზღუდულია
ისე რომ არ დაარღვიოს სინქრონიზაციის პირობები და არ მოხდეს შეცდომები სხვადასხვა პროცესის მიერ ან ფრეიმის გამოყოფის დროს ან რომელიმე ფრეიმის სვაპში გადატანის დროს.
როდესაც პროცესს page ში exec file ის ჩაწერა უნდა, filesystem ის გლობალური ლოქი არ ილოქება, რადგან exec_file ში ჩაწერა და ცვლილება არ შეიძლება, ანუ დისკზე ფაილი არ იცვლება, შესაბამისად თუნდაც რამდენიმე პროცესმა პარარელურად წაიკითხოს მათ უბრალოდ ერთი დაიგივე კონტენტი დახვდებათ, შესაბამისად დამატებითი სინქრონიზაცია არ გავაკეთეთ.
რაც შეეხება პროცესების spt(Supplemental page table ) ებს, ის ყველას თავისი აქვს და არ სჭირდება რომ პროცესმა თვითონ დალოქოს და მერე ლოქი გაანთავისუფლოს, რადგან თავის ფეიჯების hash_list თან მარტო თვითონ მუშაობს.

			 MEMORY MAPPED FILES
			 ===================

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

ზემოთ აღწერილია.
---- ALGORITHMS ----

>> C2: Describe how memory mapped files integrate into your virtual
>> memory subsystem.  Explain how the page fault and eviction
>> processes differ between swap pages and other pages.

 ისევე როგორც elf_file ის ფეიჯებში ჩატვირთვის საწყის ეტაპზე რეალურად მხოლოდ დამახსოვრება ხდება რომელ ფეიჯში ფაილის რა ნაწილი უნდა მოხვდეს და რამდენი ბაიტი, 
იგივე პრინციპი მუშაობს მემორი მეპდ ფაილებზეც. ანუ სისტემ ქოლის დროს მხოლოდ ვიმახსოვრებთ და როდესაც პროცესი რეალურად მიმართავს ამ ფეიჯებიდან რომელიმეს წაკითხვისთვის ან ჩაწესითვის, მოხდება page-fault და ჩაიწერება ფეიჯში ის შიგთავსი რომელიც მანამდე დაიმეპა.
eviction პროცესის დროს, თუ ფრეიმი რომელიც უნდა გავაგდოთ მეხიერებინდა შეიცავს mmap ფაილს, მოწმდება დაიზიანებულია თუ არა დატა ანუ pagedir_is_dirty. თუ კიდაზიანებულია, ამ
ფეიჯის კონტენტის გადაწერა მოხდება დისკზე, რადგან ფაილი განახდლეს და მასში ცვლილებები მოხდეს, თუ არაფერი არ შეცვლილა მაშინ უბრალოდ წაიშლება მეხსიერებიდან, და ხელახლა page_fault ის შემთხვევაში იგივე მისამრთზე ხელახლა წაიკითხება დისკიდან იგივე ფაილი.
>> C3: Explain how you determine whether a new file mapping overlaps
>> another segment, either at the time the mapping is created or later.
 პროცესის spt ში მოწმდება გამოყენებულია თუ არა, სისტემ ქოლში გადმოცემული buffer ის  მისამართი უკვე სხვა რომელიმე ფეიჯისთვის.

---- RATIONALE ----

>> C4: Mappings created with "mmap" have similar semantics to those of
>> data demand-paged from executables, except that "mmap" mappings are
>> written back to their original files, not to swap.  This implies
>> that much of their implementation can be shared.  Explain why your
>> implementation either does or does not share much of the code for
>> the two situations.
   სწორედ ამ ორი სიტუაციის მსგავასების გამო, სადაც ძირითადი იდეა  იყო იგივე (ფაილების დამახსოვრების, და read_bytes და zero_bytes ების დათვლის)  ეს ორი ნაწილი mmap და
demand-paged from executables იყენებს ერთი და იგივე ფუნქციებს.


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
>> students, either for future quarters or the remaining projects?

>> Any other comments?
