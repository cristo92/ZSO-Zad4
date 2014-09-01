
PLIKI W PACZCE:


ROZWIAZANIE:
	Z treści zadania - 3 warunki poprawnej bazy danych:
	1. Transakcja kończy się, gdy zostanie zatwierdzona lub porzucona przez 
	   użytkownika. Zmiany dokonane w zatwierdzonej transakcji permanentnie stają 
		się częścią bazy danych, a zmiany dokonane w porzuconej transkacji giną 
		bezpowrotnie.
	2. Transakcje widzą wyniki wcześniejszych zatwierdzonych transakcji, ale nie 
	   widzą zmian z niezatwierdzonych transakcji.
	3. “Stan świata” widziany przez transakcję nie zmienia się w trakcie jej trwania.

	Dostęp do bazy danych na zasadzie "Czytelnicy i Pisarze".
	Proces, który otwiera /dev/db wiesza się na semaforze typu "Czytelnik".
	Wszystkie operacje write są zapisywane na liście, będą wykonane, dopiero gdy
	transakcja będzie zatwierdzona(wymaga tego warunek 2.).
	Transakcje mogą być zatwierdzone dopiero jak wszystkie otwarte transakcje
	zostaną porzucone, lub wysałane do zatwierdzenia(wymaga tego warunek 3.)
	Z warunku 3. nie będzie nigdy sytuacji, że przy write wiadomo już, że
	transakcja nie ma szans się udać, dlatego u mnie write zawsze zwraca success.
	
	Tworzenie transakcji:
		- Czekam na semafor "Czytelnik"
		- Zwiększam licznik transakcji o jeden(db_counter)
		- Nadaje transakcji numer(db_counter)
		
	Zatwierdzanie transakcji:
		1. Zwalniam semafor "Czytelnik"
		2. Czekam na semafor "Pisarz"
		3. Dla każdego bloku, który zamierzam modyfikować w ramach tej transakcji,
			sprawdzam, czy jego znacznik czasu jest nie większy od identyfikatora 
			mojej transakcji.
		4. Jeśli jakikolwiek blok ma znacznik czasu większy, to porzucam transakcję.
		5. W przeciwnym razie realizuję wszystkie write'y z transakcji
		6. W trakcie modyfikowania danych bloków, ustawiam ich znacznik czasu na
		   identyfikator pierwszej transakcji, która jeszcze nie istnieje (tak więc
			wszystkie transakcje, które czekają na semadorze "Pisarz" i chcą 
			modyfikować nasz blok, zostaną odrzucone).

	Nieskończona baza danych:
		Bazę danych podzieliłem na 32-bajtowe bloki (struct db_block), które
		przechowuję w strukturze rbtree (z bibliotek kernela).
		Odczyt:
			Mając numer bloku, szukam go w drzewie, jeśli znalazłem, to go
			czytam. Jeśli nie znalazłem, to zwracam zera.
		Zapis:
			Mając numer bloku, szukam go w drzewie, jeśli znalazłem, to modyfikuję.
			Jeśli nie - to dodaję nowy, odpowiednio zmodyfikowany blok.
		Poza tym struktura db_block przechowuje 4-bajtowy znacznik czasu - używany do
		pozwalania na równoległe nieskonfliktowane zapisy.

HIERARCHIA SEMAFOROW:
	1. Czytelnicy/pisarze.		(db_rwsema)
	2. Semafor transakcyjny.	(sema w struct transaction)
	3. Semafor globalny.			(db_glsema)
	Przykład:
	Nie można blokować 2. mając zablokowany 3.

TESTOWANIE:
	W rozwiązaniu nie miało być testów, ale skoro i tak je napisałem, to
	udostępniam.
	Testy są w folderze Tests
	Uruchamiane (po zamontowaniu modułu transdb.ko) za pomocą skryptu testall.sh
	w którym również testy są opisane
	Opisane w testall.sh
	
