#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
using namespace std::chrono_literals;
using uint32 = unsigned;
using PartitionedTravelList = std::vector<int>;
using FutureDestinations = std::future<PartitionedTravelList>;
struct TravelValue {
    FutureDestinations* future = nullptr;
    FutureDestinations* Get() { return future; }
};
struct Context {
    bool created = false;
    unsigned lookups = 0;
    TravelValue value;
    bool HasValue(const char*) { return created; }
    template<class T> TravelValue* GetValue(const char*) { ++lookups; return &value; }
};
struct PlayerbotAI {
    Context* aiObjectContext = nullptr;
    void WaitForTravelSearch();
};
#include "TravelShutdownWait.inc"
struct Player { PlayerbotAI* ai = nullptr; };
PlayerbotAI* GetBotAI(Player* player) { return player ? player->ai : nullptr; }
std::atomic<bool> playerLockHeld{false};
template<class T> struct HashMapHolder {
    static int GetLock() { return 1; }
    struct ReadGuard {
        explicit ReadGuard(int) { playerLockHeld = true; }
        ~ReadGuard() { playerLockHeld = false; }
    };
};
struct ObjectAccessor {
    std::map<unsigned,Player*> players;
    const auto& GetPlayers() { return players; }
} sObjectAccessor;
struct Log { template<class... A> void outString(const char*, A...) {} } sLog;
struct WorldScript { virtual void OnShutdown() {} };
struct PlayerbotWorldScript : WorldScript {
#include "TravelShutdownHook.inc"
};
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    PlayerbotAI missing;
    missing.WaitForTravelSearch();
    Context unused;
    missing.aiObjectContext = &unused;
    missing.WaitForTravelSearch();
    Require(unused.lookups == 0, "shutdown created a previously unused value");
    unused.created = true;
    missing.WaitForTravelSearch(); // null future pointer
    FutureDestinations invalid;
    unused.value.future = &invalid;
    missing.WaitForTravelSearch();

    std::promise<void> release, entered;
    auto gate = release.get_future().share();
    std::atomic<bool> terrainAlive{true}, workerSawTerrain{false}, joinedWithoutLock{false};
    auto worker = std::async(std::launch::async, [&] {
        entered.set_value();
        gate.wait();
        joinedWithoutLock = !playerLockHeld.load();
        workerSawTerrain = terrainAlive.load();
        return PartitionedTravelList{7};
    });
    entered.get_future().wait();
    Context headlessContext; headlessContext.created = true; headlessContext.value.future = &worker;
    PlayerbotAI headlessAI; headlessAI.aiObjectContext = &headlessContext;
    Player headless{&headlessAI}, normal;
    unsigned deferredRuns = 0;
    auto deferred = std::async(std::launch::deferred, [&] { ++deferredRuns; return PartitionedTravelList{9}; });
    Context selfContext; selfContext.created = true; selfContext.value.future = &deferred;
    PlayerbotAI selfAI; selfAI.aiObjectContext = &selfContext;
    Player self{&selfAI};
    sObjectAccessor.players = {{1,&headless},{2,&normal},{3,&self},{4,nullptr}};
    PlayerbotWorldScript script;
    auto shutdown = std::async(std::launch::async, [&] { script.OnShutdown(); terrainAlive = false; });
    bool waited = shutdown.wait_for(100ms) == std::future_status::timeout;
    release.set_value();
    shutdown.get();
    Require(waited && workerSawTerrain, "map teardown overtook pending travel search");
    Require(joinedWithoutLock && !playerLockHeld, "player registry lock held while joining");
    Require(deferredRuns == 1 && worker.valid() && deferred.valid(), "shutdown consumed results or skipped deferred work");
    script.OnShutdown();
    Require(deferredRuns == 1 && worker.get()[0] == 7 && deferred.get()[0] == 9, "repeated drain changed result ownership");

    auto failure = std::async(std::launch::async, []() -> PartitionedTravelList { throw std::runtime_error("test failure"); });
    headlessContext.value.future = &failure;
    script.OnShutdown(); // wait must not rethrow or discard the exception
    Require(failure.valid(), "shutdown discarded exceptional future");
    bool retained = false;
    try { failure.get(); } catch (const std::runtime_error&) { retained = true; }
    Require(retained, "ordinary result handler lost the worker exception");
    sObjectAccessor.players.clear();
    script.OnShutdown();
    std::cout << "Native shutdown hook joins headless/self-bot travel futures before terrain teardown; results preserved\n";
}
