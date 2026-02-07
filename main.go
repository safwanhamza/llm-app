jfljf/./..?// main.go
// main entry
package main

import (
	"flag"
	"fmt"
	"strings"
	"time"
)
nmo
func main() {
	name := flag.String("name", "World", "Name to greet")
	upper := flag.Bool("upper", false, "Print the greeting in UPPERCASE")
	flag.Parse()

	greeting := fmt.Sprintf("Hello, %s! It is %s.",
		*name, time.Now().Format(time.RFC1123))

	if *upper {
		greeting = strings.ToUpper(greeting)
	}

	fmt.Println(greeting)
}
