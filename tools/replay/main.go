package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"net/http"
	"os"
	"regexp"
	"sync"
	"time"

	"github.com/alecthomas/kingpin"
)

func main() {
	logFile := parseArgs()
	tasks, err := parseLog(logFile)
	if err != nil {
		fmt.Printf("Error parsing log: %v\n", err)
		return
	}
	execute(tasks)
}

type Task struct {
	Delay    time.Duration
	Protocol string
	Request  string
}

func (t *Task) wait() {
	time.Sleep(t.Delay)
    // time.Sleep(100 * time.Millisecond)
}

func (t *Task) run() error {
	url := "http://localhost:51233" // Change to your actual endpoint
	var resp *http.Response
	var err error
	if t.Protocol == "http" {
		resp, err = http.Post(url, "application/json", bytes.NewBuffer([]byte(t.Request)))
	} else if t.Protocol == "ws" {
		return fmt.Errorf("WebSocket protocol not implemented")
	}
	if err != nil {
		return fmt.Errorf("error sending request: %v", err)
	}
	defer resp.Body.Close()
    body, err := io.ReadAll(resp.Body)
    if err != nil {
        fmt.Printf("Error reading response body: %v\n", err)
    }

	fmt.Printf("Sent request: %v, received status: %s\n%s\n", t.Request, resp.Status, body)
	return nil
}

func parseArgs() string {
	app := kingpin.New("log-replayer", "A tool to replay requests from log files.")
	logFile := app.Arg("logfile", "Path to the log file").Required().String()
	kingpin.MustParse(app.Parse(os.Args[1:]))
	return *logFile
}

func parseLog(logFile string) ([]Task, error) {
	file, err := os.Open(logFile)
	if err != nil {
		return nil, fmt.Errorf("error opening log file: %v", err)
	}
	defer file.Close()
	var tasks []Task
	var lastTimestamp time.Time
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		re := regexp.MustCompile(`(?P<timestamp>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6}) .*? (?P<protocol>http|ws) received request from work queue: (?P<json>{.*?}) ip =`)
		matches := re.FindStringSubmatch(line)
		if len(matches) > 0 {
			timestamp, err := time.Parse("2006-01-02 15:04:05.999999", matches[1])
			if err != nil {
				return nil, fmt.Errorf("error parsing timestamp: %v", err)
			}

			delay := time.Duration(0)
			if !lastTimestamp.IsZero() {
				delay = timestamp.Sub(lastTimestamp)
			}
			lastTimestamp = timestamp

			tasks = append(tasks, Task{
				Delay:    delay,
				Protocol: matches[2],
				Request:  matches[3],
			})
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("error reading log file: %v", err)
	}
	return tasks, nil
}

func execute(tasks []Task) {
    println("Executing tasks", len(tasks))
    totalDuration := time.Duration(0)
    for _, t := range tasks {
        totalDuration += t.Delay
    }
    println("Total duration", totalDuration.Minutes(), "mins")
	wg := sync.WaitGroup{}
	for _, task := range tasks {
		task.wait()
		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := task.run(); err != nil {
				fmt.Printf("Error executing task: %v\n", err)
			}
		}()
	}
	wg.Wait()
}
