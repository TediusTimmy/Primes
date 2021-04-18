// ---------------------------------------------------------------------------
// PrimeCPP.cpp : Dave's Garage Prime Sieve in C++ - No warranty for anything!
// ---------------------------------------------------------------------------

#include <chrono>
#include <ctime>
#include <iostream>
#include <bitset>
#include <map>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <memory>

using namespace std;
using namespace std::chrono;

const uint64_t DEFAULT_UPPER_LIMIT = 10'000'000LLU;

// prime_sieve
//
// Represents the data comprising the sieve (an array of N bits, where N is the upper limit prime being tested)
// as well as the code needed to eliminate non-primes from its array, which you perform by calling runSieve.

class prime_sieve
{
  private:

      vector<char> Bits;                                        // Sieve data, where 1==prime, 0==not
      /* The strange thing about this sieve is that we are only going to store odd numbers.                  */
      /* Index i in the sieve corresponds to the number (2 * i + 1).                                         */
      /* As such, we'll need to store the actual size for every place that Dave was using the vector's size. */
      uint64_t Size;
      uint64_t Threads;

   public:

      prime_sieve(uint64_t n, uint64_t t) : Bits(n >> 1, 1), Size(n), Threads(t) // Initialize all to potential primes
      {
          Bits[0] = 0; /* Except one: one is not prime. This may be a bug in Dave's code. */
      }

      ~prime_sieve()
      {
      }

      // runSieve
      //
      // Scan the array for the next factor (>2) that hasn't yet been eliminated from the array, and then
      // walk through the array crossing off every multiple of that factor.

      void runSieve()
      {
          vector<thread> threadPool;
          for (uint64_t i = 0; i < Threads; i++)
          {
              threadPool.push_back(thread([=]
              {
                  runSieve(static_cast<int64_t>(i));
              }));
          }
          for (auto &th : threadPool) 
              th.join();
      }

/*
      Lock-free threaded sieve implementation.

      One way we achieve lock-free-ness is that we don't actually compute the next prime for flagging its composites.
      We just enumerate over numbers that are more likely to be prime, and hope we don't waste too much time computing the multiples of composites.

      Consider all numbers arranged in six columns, like so:
         1  2  3  4  5  6
         7  8  9 10 11 12
        13 14 15 16 17 18
        19 20 21 22 23 24
        25 26 27 28 29 30
      By inspection, we can see that all numbers in columns 2, 3, 4, and 6 will always be composite (except for the numbers 2 and 3).
      So there exists an n for all prime numbers greater than three such that the prime number is 6n-1 or 6n+1.

      Each thread gets a different index, and then iterates over 6n-1 and 6n+1 for that index, and then it increments its index by the number of threads.
      The only special thread is the one who gets index 0: it iterates over the multiples of 3.

      The next stage of achieving lock-free-ness, and this method's fatal flaw, is selecting a unit of memory that is large enough
      so that we don't do a read-modify-write, but rather just a write.
      We only read from memory in a manner that it doesn't matter if the read is wrong
      (while it is wasteful to mark the composites of a composite as composite, it doesn't change the correctness of the algorithm),
      and we only write to memory in a manner that mis-ordered writes are still consistent
      (it doesn't matter if the three thread or five thread marks 45 as composite, what matters is that doing so doesn't unmark 40 or 42).
      The trick is to use a unit large enough that writes are atomic: my experience with bytes (on my machine) seem to work, but check your processor docs.

      The real issue is memory:
      As this uses eight times as much memory as a vector<bool>, it has less locality, and is far less performant because of it.
      And, if your processor only guarantees consistency on 4-byte or 8-byte writes, then your performance will suffer accordingly,
      as this algorithm will not be correct as programmed.
*/
      void runSieve(int64_t index)
      {
          int64_t factor = 6 * index - 1;
          int64_t q = (int) sqrt(Size);
          while (factor <= q)
          {
              if (0 == index)
              {
                  for (uint64_t num = 9; num < Size; num += 6)
                  {
                      Bits[num >> 1] = 0;
                  }
                  index += Threads;
                  factor = 6 * index - 1;
              }
              else
              {
                  if (1 == Bits[factor >> 1])
                  {
                      for (uint64_t num = factor * factor; num < Size; num += (2 * factor))
                      {
                          Bits[num >> 1] = 0;
                      }
                  }
                  factor += 2;
                  if (1 == Bits[factor >> 1])
                  {
                      for (uint64_t num = factor * factor; num < Size; num += (2 * factor))
                      {
                          Bits[num >> 1] = 0;
                      }
                  }
                  index += Threads;
                  factor = 6 * index - 1;
              }
          }
      }

      // countPrimes
      //
      // Can be called after runSieve to determine how many primes were found in total

      size_t countPrimes() const
      {
          size_t count = (Size >= 2);                   // Count 2 as prime if within range
          for (size_t i = 1; i < Bits.size(); ++i)
              if (Bits[i])
                  count++;
          return count;
      }

      // isPrime 
      // 
      // Can be called after runSieve to determine whether a given number is prime. 

      bool isPrime(uint64_t n) const
      {
          if (n & 1)
              return Bits[n >> 1];
          else
              return false;
      }

      // validateResults
      //
      // Checks to see if the number of primes found matches what we should expect.  This data isn't used in the
      // sieve processing at all, only to sanity check that the results are right when done.

      bool validateResults() const
      {
          const std::map<const uint64_t, const size_t> resultsDictionary =
          {
                {             10LLU, 4         },               // Historical data for validating our results - the number of primes
                {            100LLU, 25        },               // to be found under some limit, such as 168 primes under 1000
                {          1'000LLU, 168       },
                {         10'000LLU, 1229      },
                {        100'000LLU, 9592      },
                {      1'000'000LLU, 78498     },
                {     10'000'000LLU, 664579    },
                {    100'000'000LLU, 5761455   },
                {  1'000'000'000LLU, 50847534  },
                { 10'000'000'000LLU, 455052511 },
          };
          if (resultsDictionary.end() == resultsDictionary.find(Size))
              return false;
          return resultsDictionary.find(Size)->second == countPrimes();
      }

      // printResults
      //
      // Displays stats about what was found as well as (optionally) the primes themselves

      void printResults(bool showResults, double duration, size_t passes, size_t threads) const
      {
          if (showResults)
              cout << "2, ";

          size_t count = (Size >= 2);                   // Count 2 as prime if in range
          for (uint64_t num = 1; num <= Bits.size(); ++num)
          {
              if (Bits[num])
              {
                  if (showResults)
                      cout << (2 * num + 1) << ", ";
                  count++;
              }
          }

          if (showResults)
              cout << "\n";
          
          cout << "Passes: "  << passes << ", "
               << "Threads: " << threads << ", "
               << "Time: "    << duration << ", " 
               << "Average: " << duration/passes << ", "
               << "Limit: "   << Size << ", "
               << "Counts: "  << count << "/" << countPrimes() << ", "
               << "Valid : "  << (validateResults() ? "Pass" : "FAIL!") 
               << "\n";
      }
};

volatile auto cPasses = 0;

int main(int argc, char **argv)
{
    vector<string> args(argv + 1, argv + argc);         // From first to last argument in the argv array
    uint64_t ullLimitRequested = 0;
    auto cThreadsRequested = 0;
    auto cSecondsRequested = 0;
    auto bPrintPrimes      = false;
    auto bOneshot          = false;
    auto bQuiet            = false;

    // Process command-line args

    for (auto i = args.begin(); i != args.end(); ++i) 
    {
        if (*i == "-h" || *i == "--help") {
              cout << "Syntax: " << argv[0] << " [-t,--threads threads] [-s,--seconds seconds] [-l,--limit limit] [-1,--oneshot] [-q,--quiet] [-h] " << endl;
            return 0;
        }
        else if (*i == "-t" || *i == "--threads") 
        {
            i++;
            cThreadsRequested = (i == args.end()) ? 0 : max(1, atoi(i->c_str()));
        }
        else if (*i == "-s" || *i == "--seconds") 
        {
            i++;
            cSecondsRequested = (i == args.end()) ? 0 : max(1, atoi(i->c_str()));
        }
        else if (*i == "-l" || *i == "--limit") 
        {
            i++;
            ullLimitRequested = (i == args.end()) ? 0LL : max((long long)1, atoll(i->c_str()));
        }
        else if (*i == "-1" || *i == "--oneshot") 
        {
            i++;
            bOneshot = true;
            cThreadsRequested = 1;
        }
        else if (*i == "-p" || *i == "--print") 
        {
             bPrintPrimes = true;
        }
        else if (*i == "-q" || *i == "--quiet") 
        {
             bQuiet = true;
        }        
        else 
        {
            fprintf(stderr, "Unknown argument: %s", i->c_str());
            return 0;
        }
    }

    if (!bQuiet)
    {
        cout << "Primes Benchmark (c) 2021 Dave's Garage - http://github.com/davepl/primes" << endl;
        cout << "-------------------------------------------------------------------------" << endl;
    }

    if (bOneshot)
        cout << "Oneshot is on" << endl;

    if (bOneshot && (cSecondsRequested > 0 || cThreadsRequested > 1))   
    {
        cout << "Oneshot option cannot be mixed with second count or thread count." << endl;
        return 0;
    }

    auto cSeconds     = (cSecondsRequested ? cSecondsRequested : 5);
    auto cThreads     = (cThreadsRequested ? cThreadsRequested : thread::hardware_concurrency());
    unsigned long long llUpperLimit = (ullLimitRequested ? ullLimitRequested : DEFAULT_UPPER_LIMIT);

    if (!bQuiet)
    {
        printf("Computing primes to %llu on %d thread%s for %d second%s.\n", 
            llUpperLimit,
            cThreads,
            cThreads == 1 ? "" : "s",
            cSeconds,
            cSeconds == 1 ? "" : "s"
        );
    }
    auto tStart       = steady_clock::now();

    // We create a sieve that uses N threads.

    do
    {
        std::unique_ptr<prime_sieve>(new prime_sieve(llUpperLimit, bOneshot ? 1 : cThreads))->runSieve();
        cPasses++;
    }
    while (!bOneshot && duration_cast<seconds>(steady_clock::now() - tStart).count() < cSeconds);

    auto tEnd = steady_clock::now() - tStart;
    auto duration = duration_cast<microseconds>(tEnd).count()/1000000.0;
    
    prime_sieve checkSieve(llUpperLimit, bOneshot ? 1 : cThreads);
    checkSieve.runSieve();
    auto result = checkSieve.validateResults() ? checkSieve.countPrimes() : 0;
  
    if (!bQuiet)
        checkSieve.printResults(bPrintPrimes, duration , cPasses, cThreads);
    else
        cout << cPasses << ", " << duration / cPasses << endl;

    // On success return the count of primes found; on failure, return 0

    return (int) result;
}
