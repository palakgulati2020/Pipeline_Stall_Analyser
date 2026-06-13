#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define V 5
#define INF INT_MAX


struct Node {
    int vertex;
    int weight;
    struct Node* next;
};


struct Node* createNode(int v, int w)
{
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->vertex = v;
    newNode->weight = w;
    newNode->next = NULL;
    return newNode;
}


void addEdge(struct Node* adj[], int u, int v, int w)
{
    struct Node* newNode = createNode(v, w);

    newNode->next = adj[u];
    adj[u] = newNode;
}


void dijkstra(struct Node* adj[], int source)
{
    int dist[V];
    int visited[V];

   
    for(int i = 0; i < V; i++)
    {
        dist[i] = INF;
        visited[i] = 0;
    }

    
    dist[source] = 0;
    for(int count = 0; count < V - 1; count++)
    {
        int min = INF;
        int u = -1;

        // Find minimum distance unvisited node
        for(int i = 0; i < V; i++)
        {
            if(!visited[i] && dist[i] < min)
            {
                min = dist[i];
                u = i;
            }
        }

       
        visited[u] = 1;

        
        struct Node* temp = adj[u];

        while(temp != NULL)
        {
            int v = temp->vertex;
            int weight = temp->weight;

            
            if(!visited[v] &&
               dist[u] != INF &&
               dist[u] + weight < dist[v])
            {
                dist[v] = dist[u] + weight;
            }

            temp = temp->next;
        }
    }

   
    printf("Shortest distances from source %d:\n", source);

    for(int i = 0; i < V; i++)
    {
        printf("Node %d -> ", i);

        if(dist[i] == INF)
            printf("INF\n");
        else
            printf("%d\n", dist[i]);
    }
}

int main()
{
   
    struct Node* adj[V];

    
    for(int i = 0; i < V; i++)
        adj[i] = NULL;

    addEdge(adj, 0, 1, 2);
    addEdge(adj, 0, 2, 4);

    addEdge(adj, 1, 2, 1);
    addEdge(adj, 1, 3, 7);

    addEdge(adj, 2, 4, 3);

    addEdge(adj, 4, 3, 2);

    int source = 0;

    dijkstra(adj, source);

    return 0;
}