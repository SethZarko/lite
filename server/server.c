#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define SIZE 1024  // Buffer size for reading data
#define PORT 8000  // Port number the server will listen on
#define BACKLOG 10 // Maximum number of pending connections

// Function prototypes
void getFileURL(char *route, char *fileURL);
void getMimeType(char *file, char *mime);
void handleSignal(int signal);
void getTimeString(char *buf);

// Global variables for socket connections
int serverSocket;
int clientSocket;
char *request;

int main()
{
  // Set up signal handling for SIGINT (Ctrl+C) to properly shut down the server
  signal(SIGINT, handleSignal);

  // Server address configuration
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;                     // Address family (IPv4)
  serverAddress.sin_port = htons(PORT);                   // Port number (converted to network byte order)
  serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Bind to localhost

  // Create the server socket
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  // Error Handlers
  if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    printf("Error: The server is not bound to the address.\n");
    return 1;
  }

  if (listen(serverSocket, BACKLOG) < 0)
  {
    printf("Error: The server is not listening.\n");
    return 1;
  }
  // Get and print server address information
  char hostBuffer[NI_MAXHOST], serviceBuffer[NI_MAXSERV];
  int error = getnameinfo((struct sockaddr *)&serverAddress, sizeof(serverAddress), hostBuffer, sizeof(hostBuffer), serviceBuffer, sizeof(serviceBuffer), 0);

  if (error != 0)
  {
    printf("Error: %s\n", gai_strerror(error));
    return 1;
  }

  printf("\nServer is listening on http://%s:%s/\n\n", hostBuffer, serviceBuffer);

  // Main server loop - infinitely open and listening to accept client connections
  while (1)
  {
    // Allocate memory for the incoming request
    request = (char *)malloc(SIZE * sizeof(char));
    if (request == NULL)
    {
      perror("Failed to allocate memory");
      continue;
    }

    // Accept a client connection
    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket < 0)
    {
      perror("Failed to accept connection");
      free(request);
      continue;
    }

    // Read the client's request
    ssize_t bytesRead = read(clientSocket, request, SIZE - 1);
    if (bytesRead < 0)
    {
      perror("Failed to read request");
      close(clientSocket);
      free(request);
      continue;
    }
    request[bytesRead] = '\0'; // Null-terminate the request

    // Parse the HTTP method and route from the request
    char method[10], route[100];
    sscanf(request, "%s %s", method, route);
    printf("Method: %s, Route: %s\n", method, route);

    free(request); // Free the allocated memory for request

    // Handle only GET requests
    if (strcmp(method, "GET") != 0)
    {
      const char response[] = "HTTP/1.1 400 Bad Request\r\n\n";
      send(clientSocket, response, sizeof(response) - 1, 0);
    }
    else
    {
      // Determine the file URL based on the route
      char fileURL[200];
      getFileURL(route, fileURL);

      // Open the requested file
      FILE *file = fopen(fileURL, "r");
      if (!file)
      {
        const char response[] = "HTTP/1.1 404 Not Found\r\n\n";
        send(clientSocket, response, sizeof(response) - 1, 0);
      }
      else
      {
        // Handle file serving
        fseek(file, 0, SEEK_END);  // Move to end of file
        long fsize = ftell(file);  // Get/Calculate file size
        fseek(file, 0, SEEK_SET);  // Move back to start of file

        // Prepare the response header
        char resHeader[SIZE];
        char timeBuf[100];
        getTimeString(timeBuf); // Get the current date and time for the response header

        char mimeType[32];
        getMimeType(fileURL, mimeType); // Determine the MIME type of the file

        // Format the response header
        sprintf(resHeader, "HTTP/1.1 200 OK\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", timeBuf, mimeType, fsize); 
        
        // Allocate memory for the response (header + file content)
        char *resBuffer = (char *)malloc(fsize + strlen(resHeader));
        if (resBuffer == NULL)
        {
          perror("Failed to allocate memory");
          fclose(file);
          close(clientSocket);
          continue;
        }

        // Copy header and file content to response buffer
        strcpy(resBuffer, resHeader);
        fread(resBuffer + strlen(resHeader), fsize, 1, file);

        // Send the response to the client
        send(clientSocket, resBuffer, fsize + strlen(resHeader), 0);
        free(resBuffer);  // Free allocated memory for response buffer
        fclose(file);     // Close the file
      }
    }
    close(clientSocket); // Close the client socket
    printf("\n");
  }
}
// FUNCTIONS // 

// Function to construct the file path from the route
void getFileURL(char *route, char *fileURL)
{
  // Remove query parameters
  char *question = strrchr(route, '?');
  if (question)
    *question = '\0';

  // Append index.html if route ends with '/'
  if (route[strlen(route) - 1] == '/')
    strcat(route, "index.html");

  // Construct the file URL
  strcpy(fileURL, "../client/dist/client/browser");
  strcat(fileURL, route);

  // Add .html extension if no file extension is present
  const char *dot = strrchr(fileURL, '.');
  if (!dot || dot == fileURL)
  {
    strcat(fileURL, ".html");
  }
}

// Function to determine the MIME type of a file based on its extension
void getMimeType(char *file, char *mime)
{
  const char *dot = strrchr(file, '.');
  if (dot == NULL)
    strcpy(mime, "text/html");
  else if (strcmp(dot, ".html") == 0)
    strcpy(mime, "text/html");
  else if (strcmp(dot, ".css") == 0)
    strcpy(mime, "text/css");
  else if (strcmp(dot, ".js") == 0)
    strcpy(mime, "application/javascript");
  else if (strcmp(dot, ".jpg") == 0)
    strcpy(mime, "image/jpeg");
  else if (strcmp(dot, ".png") == 0)
    strcpy(mime, "image/png");
  else if (strcmp(dot, ".gif") == 0)
    strcpy(mime, "image/gif");
  else
    strcpy(mime, "text/html");  // Default MIME type for unknown extensions
}

// Signal handler to clean up and shut down the server gracefully
void handleSignal(int signal)
{
  if (signal == SIGINT)
  {
    printf("\nShutting down server...\n");
    close(clientSocket);
    close(serverSocket);
    if (request != NULL)
      free(request);
    exit(0);
  }
}

// Function to get the current date and time as a string for the response header
void getTimeString(char *buf)
{
  time_t now = time(0);                                            // Get current time
  struct tm tm = *gmtime(&now);                                    // Convert to GMT time
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);     // Format time string

}
