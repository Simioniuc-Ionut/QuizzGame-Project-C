# Quizz--Server-Client-
  General Overview:
  
The purpose of the project is to implement a server that allows the connection of n clients securely, swiftly, and synchronized. Upon calling the "START" function, no other clients will be able to connect to the respective Quiz. Questions will be retrieved from the SQLite database and sent concurrently by the server to all clients, each having n seconds to respond to each question. If there is no existing database, the server will create a standard one. At the end of the game, a Leaderboard will be presented with the winners of the game.

  Project Objectives:

-Secure and efficient connection of n clients to the server.

-Creation of a concurrent multithreading server.

-Implementation of the question table using the SQLite database.

-Sending a Leaderboard to the client including multiple details about each registered player.

-Creation of a graphical timer in the client.

-The server will be capable of running the Quiz session again.
