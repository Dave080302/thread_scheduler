Nume: Anghel David-Tudor
Grupa: 322CC

Am creat o structura de tip so_thread, care simuleaza un thread efectiv, avand cele mai importante campuri
ale unui structuri de tip thread: cuanta, prioritatea, procesul pe care il asteapta,starea, restul 
fiind mecanisme de sincronizare sau specifice task-ului acestei teme. De asemenea, am creeat o structura de tip 
scheduler, care contine o coada de prioritati a thread-urilor din READY, un vector cu toate thread-urile din sistem,
time_slice-ul maxim al fiecarui thread, numarul de evenimente de tip io, daca programul a fost deja rulat si nr 
thread-urilor din sistem. In so_init, initializam toate campurile schedulerului, fiecare thread in parte fiind
initializat folosind so_fork. Pentru sincronizare, am folosit variabile de conditie (pthread_cond) si mutex-uri.
Dupa apelul pthread_create, am mai pus un wait, iar in start_thread am pus un signal in plus pentru a ma asigura 
ca imediat dupa pthread_create va rula start_thread. Dupa ce a fost creat, un thread verifica daca trebuie sa intre
in fata thread-ului din RUNNING(priorit mai mare, cuanta expirata sau thread terminat) sau daca acesta exista.
In so_wait, punem thread-ul care ruleaza in starea WAITING, asteptand dupa un proces de tip IO, si trecem primul 
thread din coada de prioritati in starea RUNNING. In so_signal, deblocam toate thread-urile care asteapta dupa 
evenimentul respectiv si verificam daca trebuie rulat alt thread. Coada de prioritati este creata dupa algoritmul
round-robin cu prioritati, adica le ordonam dupa prioritati, iar apoi in ordinea in care au fost introduse in coada.
Tema este foarte utila in intelegerea thread-urilor si a rularii proceselor multi-threaded. Cred ca ar fi fost putin
mai interesanta daca ar fi trebuit sa calculam noi prioritatea vazand daca un thread este mai IO sau CPU expensive.
Consider ca implementarea este destul de eficienta, nu stiu ce as fi putut imbunatati. Este implementat intregul 
enunt al temei, fara functionalitati extra. Am intampinat dificultati la inceput, cand imi rula restul programului
in loc de start_thread, iar apoi dupa ce am reusit sa fac tema de 95/95, cand am uploadat-o pe vmchecker, fix-ul
pe care il facusem pentru local nu functiona si acolo si a trebuit sa gasesc un altul trecand toata implementarea
temei pe masina virtuala pentru a testa. Biblioteca generata este de tip shared, se ruleaza cu LD_LIBRARY_PATH=. 
./run_test.sh
Resurse: Documentatie pthread_cond, pthread_mutex, laboratoare SO, Unikraft.
