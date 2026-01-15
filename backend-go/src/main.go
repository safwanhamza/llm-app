package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"github.com/gin-gonic/gin"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	ai_pb "github.com/simulation-app/backend-go/pb/ai"
	sim_pb "github.com/simulation-app/backend-go/pb/simulation"
)

var (
	computeClient sim_pb.SimulationServiceClient
	aiClient      ai_pb.OptimizerServiceClient
	dataDir       = "/app/data"
)

func main() {
	// Connect to Compute Service (C++)
	computeConn, err := grpc.Dial("compute-service:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("did not connect to compute service: %v", err)
	}
	defer computeConn.Close()
	computeClient = sim_pb.NewSimulationServiceClient(computeConn)

	// Connect to AI Service (Python)
	aiConn, err := grpc.Dial("ai-service:50052", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatalf("did not connect to ai service: %v", err)
	}
	defer aiConn.Close()
	aiClient = ai_pb.NewOptimizerServiceClient(aiConn)

	// Setup Data Directory
	if err := os.MkdirAll(dataDir, 0755); err != nil {
		log.Fatalf("failed to create data directory: %v", err)
	}

	// Setup Router
	r := gin.Default()

	// CORS Middleware
	r.Use(func(c *gin.Context) {
		c.Writer.Header().Set("Access-Control-Allow-Origin", "*")
		c.Writer.Header().Set("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
		c.Writer.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	})

	api := r.Group("/api")
	{
		api.POST("/simulate/heat", handleHeatSimulation)
		api.POST("/simulate/nbody", handleNBodySimulation)
		api.POST("/optimize/heat", handleOptimizeHeat)
		api.POST("/optimize/nbody", handleOptimizeNBody)
		api.GET("/results/:filename", handleGetResult)
		api.GET("/results", handleListResults)
	}

	log.Println("Go Backend listening on port 8080")
	r.Run(":8080")
}

func handleHeatSimulation(c *gin.Context) {
	var params sim_pb.HeatParams
	if err := c.ShouldBindJSON(&params); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	result, err := computeClient.SolveHeatEquation(ctx, &params)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	filename := fmt.Sprintf("heat_%d.json", time.Now().Unix())
	saveResult(filename, result)

	c.JSON(http.StatusOK, gin.H{"filename": filename, "result": result})
}

func handleNBodySimulation(c *gin.Context) {
	var params sim_pb.NBodyParams
	if err := c.ShouldBindJSON(&params); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	result, err := computeClient.SimulateNBody(ctx, &params)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	filename := fmt.Sprintf("nbody_%d.json", time.Now().Unix())
	saveResult(filename, result)

	c.JSON(http.StatusOK, gin.H{"filename": filename, "result": result})
}

func handleOptimizeHeat(c *gin.Context) {
	var goal ai_pb.HeatGoal
	if err := c.ShouldBindJSON(&goal); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	params, err := aiClient.OptimizeHeatParams(ctx, &goal)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, params)
}

func handleOptimizeNBody(c *gin.Context) {
	var goal ai_pb.NBodyGoal
	if err := c.ShouldBindJSON(&goal); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	params, err := aiClient.OptimizeNBodyParams(ctx, &goal)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, params)
}

func saveResult(filename string, data interface{}) {
	bytes, _ := json.Marshal(data)
	ioutil.WriteFile(filepath.Join(dataDir, filename), bytes, 0644)
}

func handleGetResult(c *gin.Context) {
	filename := c.Param("filename")
	path := filepath.Join(dataDir, filename)

	if _, err := os.Stat(path); os.IsNotExist(err) {
		c.JSON(http.StatusNotFound, gin.H{"error": "File not found"})
		return
	}

	c.File(path)
}

func handleListResults(c *gin.Context) {
	files, err := ioutil.ReadDir(dataDir)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	var filenames []string
	for _, f := range files {
		filenames = append(filenames, f.Name())
	}
	c.JSON(http.StatusOK, filenames)
}
