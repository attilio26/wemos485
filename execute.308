<?php
//23-02-2018
//started on 01-06-2017
// La app di Heroku si puo richiamare da browser con
//			https://wemos485.herokuapp.com/


/*API key = 315635925:AAFHPIBs9_aGXqv2_IBQPVJWYcAFM-tWsWU

da browser request ->   https://wemos485.herokuapp.com/register.php
           answer  <-   {"ok":true,"result":true,"description":"Webhook is already set"}
In questo modo invocheremo lo script register.php che ha lo scopo di comunicare a Telegram
l’indirizzo dell’applicazione web che risponderà alle richieste del bot.

da browser request ->   https://api.telegram.org/bot315635925:AAFHPIBs9_aGXqv2_IBQPVJWYcAFM-tWsWU/getMe
           answer  <-   {"ok":true,"result":{"id":315635925,"first_name":"wemos485","username":"wemos485_bot"}}

*********
	https://wemos485.herokuapp.com/register.php
											è questo che permette a Telegram di essere linkato a Heroku 
	
*********			   
		   
riferimenti:
https://gist.github.com/salvatorecordiano/2fd5f4ece35e75ab29b49316e6b6a273
https://www.salvatorecordiano.it/creare-un-bot-telegram-guida-passo-passo/

--- http://www.andreaminini.com/telegram/come-pubblicare-automaticamente-su-telegram-tramite-ifttt

*/
$content = file_get_contents("php://input");
$update = json_decode($content, true);

if(!$update)
{
  exit;
}

$message = isset($update['message']) ? $update['message'] : "";
$messageId = isset($message['message_id']) ? $message['message_id'] : "";
$chatId = isset($message['chat']['id']) ? $message['chat']['id'] : "";
$firstname = isset($message['chat']['first_name']) ? $message['chat']['first_name'] : "";
$lastname = isset($message['chat']['last_name']) ? $message['chat']['last_name'] : "";
$username = isset($message['chat']['username']) ? $message['chat']['username'] : "";
$date = isset($message['date']) ? $message['date'] : "";
$text = isset($message['text']) ? $message['text'] : "";

// pulisco il messaggio ricevuto togliendo eventuali spazi prima e dopo il testo
$text = trim($text);
// converto tutti i caratteri alfanumerici del messaggio in minuscolo
$text = strtolower($text);

header("Content-Type: application/json");

//ATTENZIONE!... Tutti i testi e i COMANDI contengono SOLO lettere minuscole
$response = '';

// Note our use of ===.  Simply == would not work as expected
// because the position of '/' was the 0th (first) character
if(strpos($text, "/start") === 0 || $text=="ciao" || $text == "help"){
	$response = "Ciao $firstname, benvenuto! \n List of commands : \n /bed -> Lettura stazione1
	/lunch -> Lettura stazione2  \n /kitch -> Lettura stazione3
	/livg -> Lettura stazione4 \n /boil  -> Lettura stazione5 ... su bus RS485
	/inf -> parametri del messaggio";
}
//------------
//<-- Lettura parametri slave1
elseif(strpos($text,"bed")){
	$response = file_get_contents("http://dario95.ddns.net:8083/letto");
}
//<-- Lettura parametri slave2
elseif(strpos($text,"lunch")){
	$response = file_get_contents("http://dario95.ddns.net:8083/pranzo");
}
//<-- Lettura parametri slave3
elseif(strpos($text,"kitch")){
	$response = file_get_contents("http://dario95.ddns.net:8083/cucina");
}
//<-- Lettura parametri slave4
elseif(strpos($text,"livg")){
	$response = file_get_contents("http://dario95.ddns.net:8083/salotto");
}
//<-- Lettura parametri slave5
elseif(strpos($text,"boil")){
	$response = file_get_contents("http://dario95.ddns.net:8083/caldaia");
}
//------------
//<-- Accensione rele1 su ESP01_lamp  	Cambiata la porta del router dopo tentativi da 8082  31/10/2017
elseif(strpos($text,"fish1")){
	$response = file_get_contents("http://dario95.ddns.net:28082/gpio1/1");
}
//<-- Spegnimento rele1 su ESP01_lamp	Cambiata la porta del router dopo tentativi da 8082  31/10/2017
elseif(strpos($text,"fish0")){
	$response = file_get_contents("http://dario95.ddns.net:28082/gpio1/0");
}
//<-- Accensione rele2 su ESP01_lamp	Cambiata la porta del router dopo tentativi da 8082  31/10/2017
elseif(strpos($text,"lob1")){
	$response = file_get_contents("http://dario95.ddns.net:28082/gpio2/1");
}
//<-- Spegnimento rele2 su ESP01_lamp	Cambiata la porta del router dopo tentativi da 8082  31/10/2017
elseif(strpos($text,"lob0")){
	$response = file_get_contents("http://dario95.ddns.net:28082/gpio2/0");
}
//------------
//<-- Accensione rele1 su ESPlogger	Cambiata la porta del router dopo tentativi da 8081  31/10/2017
elseif(strpos($text,"balc1")){
	$response = file_get_contents("http://dario95.ddns.net:28081?pin=10");
}
//<-- Spegnimento rele1 su ESPlogger	Cambiata la porta del router dopo tentativi da 8081  31/10/2017
elseif(strpos($text,"balc0")){
	$response = file_get_contents("http://dario95.ddns.net:28081?pin=11");
}
//<-- Accensione rele2 su ESPlogger	Cambiata la porta del router dopo tentativi da 8081  31/10/2017
elseif(strpos($text,"entr1")){
	$response = file_get_contents("http://dario95.ddns.net:28081?pin=12");
}
//<-- Spegnimento rele2 su ESPlogger	Cambiata la porta del router dopo tentativi da 8081  31/10/2017
elseif(strpos($text,"entr0")){
	$response = file_get_contents("http://dario95.ddns.net:28081?pin=13");
}

//<-- Manda a video la risposta completa
elseif($text=="/inf"){
//	$response = "chatId ".$chatId. "   messId ".$messageId. "  user ".$username. "   lastname ".$lastname. "   firstname ".$firstname. "   ".
//	"TS  ".$date."   testo  ".$text;
	$response = file_get_contents("http://dario95.ddns.net:28081?");
}
else
{
	$response = "Unknown command!";			//<---Capita quando i comandi contengono lettere maiuscole
}

// la mia risposta è un array JSON composto da chat_id, text, method
// chat_id mi consente di rispondere allo specifico utente che ha scritto al bot
// text è il testo della risposta
$parameters = array('chat_id' => $chatId, "text" => $response);
$parameters["method"] = "sendMessage";
// imposto la keyboard
$parameters["reply_markup"] = '{ "keyboard": [["/bed", "/lunch","/kitch","/livg","/boil"],["/fish1","/fish0","/lob1","/lob0"],["/balc1","/balc0","/entr1","/entr0","/inf"]], "one_time_keyboard": false}';
// converto e stampo l'array JSON sulla response
echo json_encode($parameters);
?>