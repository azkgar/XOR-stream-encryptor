# XOR-stream-encryptor
Simple C utility that performs a simple XOR cryptographic transform on a given set of data that builds on UNIX platforms. This utility takes advantage of multi-core or multi-processor machines.


## Testing & Verification

To guarantee production-grade execution safety, memory optimization, and thread concurrency boundaries, an automated integration suite was built and executed. 

### Verification Strategy
1. **End-to-End Differential Testing:** Verified that $Data \oplus Key \oplus Key == Data$ holds flawless integrity across scaling file boundaries (up to 10MB).
2. **Concurrency Assertions:** Scaled threads up to 12 concurrent workers to verify condition variable signaling and lock synchronization boundaries.
3. **Dynamic Analysis:** * **AddressSanitizer (ASan):** Executed to prove 100% leak-free heap memory operations and strict spatial/temporal memory boundaries.
   * **ThreadSanitizer (TSan):** Executed to guarantee absolute protection against race conditions, structural deadlocks, or thread hazards.

The validated execution logs are archived inside `testing/test_execution_log.txt`.