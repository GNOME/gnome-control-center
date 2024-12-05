# Testing the Printers panel without a printer

The cups package in most distributions includes the lpadmin tool. With that you can create a "dummy" printer that redirects its output to /dev/null.

```bash
sudo lpadmin -p FakePrinter -E -v file:/dev/null
```
