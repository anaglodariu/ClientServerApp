server.c:
    - se face implementarea serverului (care are rolul de broker intre clientii udp si tcp)
    - in poll-ul creat vom avea initial 3 fd (unul pentru socketul tcp care asculta
    pentru noi conexiuni, unul pentru socketul udp pe care vom primi date si unul 
    pentru stdin pentru a putea citi de la tastatura)
    - de la tastatura putem primi doar "exit", caz in care se inchide serverul si toti
    clientii tcp conectati la el
    - daca se primeste un mesaj pe socketul udp, va fi un mesaj cu topic, tip de date
    si continut -> mesajul il voi retine intr-o structura creata (struct udp_packet) ;
    in structura udp_packet am mai adaugat si 2 campuri pentru portul clientului udp si
    ip-ul lui pentru atunci cand trimitem mesajul catre clientul tcp (avem nevoie de acestea
    la afisare)
    - atat poll_fds in care retin fd-urile, cat si connected_clients in care retin
    informatii despre clientii tcp deja conectati sunt vectori alocati dinamic 
    (in cazul in care nu mai avem spatiu, realocam mai mult spatiu)
    - daca se primeste ceva pe socketul tcp care asculta pentru noi conexiuni, atunci
    se adauga noul socket in poll_fds
    - daca se primeste un mesaj de la vreun client tcp conectat, acesta poate fi de 2 feluri:
        - un mesaj cu informatii despre clientul tcp (id, ip, port, socket) (pe care le retin in
        structura struct info_tcp_client)
        - mesaje de tip subscribe/unsubscribe sau exit (pe care le retin in structura struct tcp_message)
    - daca nu era deja adaugat, adaugam clientul in vectorul connected_clients
    - am incercat sa imi creez un vector de structuri de tipul struct topicList,
    in care, pentru fiecare index al vectorului (corespunzator unui socket), voi retine
    o lista de topicuri la care este abonat clientul respectiv, insa primesc timeout la
    rularea checkerului -> DE ACEEA TRIMIT MESAJELE LA TOTI CLIENTII, INDIFERENT DE TOPICURILE LA CARE SUNT
    ABONATI (ceea ce nu e bine, dar nu am reusit sa gasesc o solutie pentru a rezolva problema)
    - partea de store and forward nu am implementat-o

subscriber.c
    - se face implementarea clientului tcp
    - in poll-ul creat vom avea 2 fd (unul pentru socketul tcp pe care vom primi
    mesaje de la server si unul pentru stdin pentru a putea citi de la tastatura)
    - de la server putem primi mesaje de tipul exit (struct tcp_message) sau de tipul
    topic + tip de date + continut (struct udp_packet)
    - de la tastatura putem primi exit, subscribe... sau unsubscribe..., mesaje care vor 
    fi trimise catre server

Referinte: labul 7 de tcp(de unde am preluat ideile/codul pentru common.c/h si helpers.h) si 6 de udp