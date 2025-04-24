package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"sort"
	"strings"
	"time"

	"github.com/gocql/gocql"
)

// Credentials represents the DB connection credentials from the JSON file
type Credentials struct {
	Hosts    []string `json:"hosts"`
	Keyspace string   `json:"keyspace"`
	Username string   `json:"username"`
	Password string   `json:"password"`
}

func main() {
	// Read credentials from file
	creds, err := readCredentials("credentials.json")
	if err != nil {
		log.Fatalf("Failed to read credentials: %v", err)
	}

	// Connect to ScyllaDB
	cluster := gocql.NewCluster(creds.Hosts...)
	cluster.Keyspace = creds.Keyspace
	cluster.Authenticator = gocql.PasswordAuthenticator{
		Username: creds.Username,
		Password: creds.Password,
	}
	cluster.Timeout = 5 * time.Second

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatalf("Failed to connect to ScyllaDB: %v", err)
	}
	defer session.Close()
	fmt.Println("Connected to ScyllaDB successfully")

	// Query system_traces.events table
	if err := querySystemTracesEvents(session); err != nil {
		log.Fatalf("Failed to query system_traces.events: %v", err)
	}
}

// readCredentials reads and parses the credentials file
func readCredentials(filename string) (*Credentials, error) {
	data, err := os.ReadFile(filename)
	if err != nil {
		return nil, fmt.Errorf("could not read credentials file: %w", err)
	}

	var creds Credentials
	if err := json.Unmarshal(data, &creds); err != nil {
		return nil, fmt.Errorf("could not parse credentials file: %w", err)
	}

	return &creds, nil
}

// Event represents a collection of trace events for a single session
type Event struct {
	SessionID gocql.UUID
	Entries   []EventEntry
}

// EventEntry represents a single trace event entry
type EventEntry struct {
	EventID       gocql.UUID
	Activity      string
	SourceElapsed int
	Thread        string
	Duration      int // Will be calculated later
}

// querySystemTracesEvents queries all data from the system_traces.events table
func querySystemTracesEvents(session *gocql.Session) error {
	fmt.Println("Querying system_traces.events table...")

	// Create a query to select specific fields from system_traces.events
	query := session.Query(`
		SELECT session_id, event_id, activity, source_elapsed, thread 
		FROM system_traces.events
	`)

	iter := query.Iter()

	// Define variables to hold the data
	var sessionID gocql.UUID
	var eventID gocql.UUID
	var activity string
	var sourceElapsed int
	var thread string

	// Map to store events by session ID
	eventsBySession := make(map[string]*Event)

	// Scan and collect rows
	for iter.Scan(&sessionID, &eventID, &activity, &sourceElapsed, &thread) {
		sessionIDStr := sessionID.String()
		
		// Create new Event if this session hasn't been seen before
		if _, exists := eventsBySession[sessionIDStr]; !exists {
			eventsBySession[sessionIDStr] = &Event{
				SessionID: sessionID,
				Entries:   []EventEntry{},
			}
		}
		
		// Add entry to the corresponding event
		entry := EventEntry{
			EventID:       eventID,
			Activity:      activity,
			SourceElapsed: sourceElapsed,
			Thread:        thread,
		}
		eventsBySession[sessionIDStr].Entries = append(eventsBySession[sessionIDStr].Entries, entry)
	}

	if err := iter.Close(); err != nil {
		return fmt.Errorf("error in query: %w", err)
	}

	// Process events
	events := make([]*Event, 0, len(eventsBySession))
	for _, event := range eventsBySession {
		events = append(events, event)
	}

	// Calculate durations and find time-consuming activities
	processEvents(events)

	return nil
}

// processEvents sorts entries by sourceElapsed, calculates durations, and prints time-consuming activities
func processEvents(events []*Event) {
	fmt.Println("Processing events to calculate durations...")
	
	// Activity durations across all events and map to store one example of full activity text
	activityDurations := make(map[string]int)
	activityExamples := make(map[string]string) // Maps prefix to a full activity example

	sum := 0
	
	for _, event := range events {
		// Sort entries by source_elapsed
		sort.Slice(event.Entries, func(i, j int) bool {
			return event.Entries[i].SourceElapsed < event.Entries[j].SourceElapsed
		})
		
		// Calculate durations
		for i := 1; i < len(event.Entries); i++ {
			event.Entries[i].Duration = event.Entries[i].SourceElapsed - event.Entries[i - 1].SourceElapsed
			
			// Aggregate durations by activity
			// Use the first 20 symbols of the activity to group similar activities
			activity := event.Entries[i].Activity
			activityPrefix := truncate(activity, 20)
			activityDurations[activityPrefix] += event.Entries[i].Duration
			
			// Store an example of the full activity text if we haven't seen this prefix before
			if _, exists := activityExamples[activityPrefix]; !exists {
				activityExamples[activityPrefix] = activity
			}
		}
		sum += event.Entries[len(event.Entries)-1].SourceElapsed
	}

	fmt.Println("Total duration:", sum, "us")
	
	// Convert to slice for sorting
	type activityDuration struct {
		Activity     string // Full activity example
		ActivityKey  string // Truncated key used for grouping
		Duration     int
	}
	
	durationSlice := make([]activityDuration, 0, len(activityDurations))
	for activityKey, duration := range activityDurations {
		fullActivity := activityExamples[activityKey] // Get the full activity example
		durationSlice = append(durationSlice, activityDuration{
			Activity:    fullActivity,
			ActivityKey: activityKey,
			Duration:    duration,
		})
	}
	
	// Sort by duration (descending)
	sort.Slice(durationSlice, func(i, j int) bool {
		return durationSlice[i].Duration > durationSlice[j].Duration
	})
	
	// Print most time-consuming activities
	fmt.Println("\nMost Time-Consuming Activities:")
	fmt.Printf("%-50s | %-15s\n", "ACTIVITY", "TOTAL DURATION (μs)")
	fmt.Println(strings.Repeat("-", 70))
	
	for _, ad := range durationSlice {
		// Truncate the full activity if it's too long for display
		displayActivity := truncateForDisplay(ad.Activity, 50)
		fmt.Printf("%-50s | %-15d\n", displayActivity, ad.Duration)
	}
}

// truncate shortens a string if it's longer than max length and adds "..."
// Used for creating activity keys for grouping
func truncate(s string, maxLen int) string {
	if len(s) <= maxLen {
		return s
	}
	return s[:maxLen-3] + "..."
}

// truncateForDisplay formats a string for display purposes
func truncateForDisplay(s string, maxLen int) string {
	if len(s) <= maxLen {
		return s
	}
	return s[:maxLen-3] + "..."
}
