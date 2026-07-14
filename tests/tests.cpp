#include <cpr/api.h>
#include <ctime>
#include <future>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <../json.hpp>
#include <spdlog/sinks/null_sink.h>
#include <string>
#include <vector>
#include <thread>

#include "../basegraphics.h"
#include "../ledfeature.h"

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

atomic<uint32_t> LEDFeature::_nextId{0};
atomic<uint32_t> SocketChannel::_nextId{0};
shared_ptr<spdlog::logger> logger = spdlog::null_logger_mt("tests");

const std::string BASE_URL = "http://localhost:7777/api";

const int stddelay = 5;

const auto jsonHeader = cpr::Header{{"Content-Type", "application/json"}};
const auto noPersistParam = cpr::Parameters{{"nopersist", "1"}};

class DummyEffectsManager : public IEffectsManager
{
public:
    void AddEffect(shared_ptr<ILEDEffect>) override {}
    void RemoveEffect(shared_ptr<ILEDEffect>&) override {}
    void StartCurrentEffect(ICanvas&) override {}
    void SetCurrentEffect(size_t, ICanvas&) override {}
    size_t GetCurrentEffect() const override { return 0; }
    size_t EffectCount() const override { return 0; }
    vector<shared_ptr<ILEDEffect>> Effects() const override { return {}; }
    void UpdateCurrentEffect(ICanvas&, microseconds) override {}
    void NextEffect() override {}
    void PreviousEffect() override {}
    string CurrentEffectName() const override { return ""; }
    void ClearEffects() override {}
    bool WantsToRun() const override { return false; }
    void WantToRun(bool) override {}
    bool IsRunning() const override { return false; }
    void Start(ICanvas&) override {}
    void Stop() override {}
    void SetFPS(uint16_t fps) override { _fps = fps; }
    uint16_t GetFPS() const override { return _fps; }
    void SetEffects(vector<shared_ptr<ILEDEffect>>) override {}
    void SetCurrentEffectIndex(int) override {}

private:
    uint16_t _fps = 30;
};

class FeatureMappingCanvas : public ICanvas
{
public:
    FeatureMappingCanvas(uint32_t width, uint32_t height)
        : _graphics(width, height)
    {
    }

    uint32_t Id() const override { return 1; }
    uint32_t SetId(uint32_t id) override { return id; }
    string Name() const override { return "Feature Mapping Canvas"; }
    uint32_t AddFeature(shared_ptr<ILEDFeature> feature) override
    {
        feature->SetCanvas(this);
        _features.push_back(feature);
        return feature->Id();
    }
    bool RemoveFeatureById(uint16_t) override { return false; }
    vector<shared_ptr<ILEDFeature>> Features() override { return _features; }
    const vector<shared_ptr<ILEDFeature>> Features() const override { return _features; }
    ILEDGraphics& Graphics() override { return _graphics; }
    const ILEDGraphics& Graphics() const override { return _graphics; }
    IEffectsManager& Effects() override { return _effects; }
    const IEffectsManager& Effects() const override { return _effects; }

private:
    BaseGraphics _graphics;
    DummyEffectsManager _effects;
    vector<shared_ptr<ILEDFeature>> _features;
};


class APITest : public ::testing::Test
{
protected:

    void SetUp() override
    {
        // Clean up any existing test data
        cleanupTestData();
    }

    void TearDown() override
    {
        // Clean up after tests
        cleanupTestData();
    }

    void cleanupTestData()
    {
    }
};

// Test Controller endpoint
TEST_F(APITest, GetController)
{
    auto response = cpr::Get(cpr::Url{BASE_URL + "/controller"}, noPersistParam);
    ASSERT_EQ(response.status_code, 200);

    auto data = json::parse(response.text);
    ASSERT_TRUE(data.contains("controller"));
}
// Test Sockets endpoints
TEST_F(APITest, GetSockets)
{
    auto response = cpr::Get(cpr::Url{BASE_URL + "/sockets"}, noPersistParam);
    ASSERT_EQ(response.status_code, 200);

    auto jsonResponse = json::parse(response.text);
    ASSERT_TRUE(jsonResponse.contains("sockets")); // Ensure "sockets" key exists
    ASSERT_TRUE(jsonResponse["sockets"].is_array()); // Verify "sockets" is an array
}


TEST_F(APITest, GetSpecificSocket)
{
    // First get all sockets
    auto response = cpr::Get(cpr::Url{BASE_URL + "/sockets"}, noPersistParam);
    ASSERT_EQ(response.status_code, 200);

    // Parse the response as a JSON object
    auto jsonResponse = json::parse(response.text);

    // Ensure the "sockets" key exists and is an array
    ASSERT_TRUE(jsonResponse.contains("sockets"));
    ASSERT_TRUE(jsonResponse["sockets"].is_array());

    // Access the sockets array
    auto sockets = jsonResponse["sockets"];

    // Check if the array is not empty
    if (!sockets.empty())
    {
        // Get the first socket's ID
        int firstSocketId = sockets[0]["id"].get<int>();

        // Fetch data for the specific socket
        auto socketResponse = cpr::Get(cpr::Url{BASE_URL + "/sockets/" + std::to_string(firstSocketId)},
                                       noPersistParam);
        ASSERT_EQ(socketResponse.status_code, 200);

        // Parse the response for the specific socket
        auto socketData = json::parse(socketResponse.text);

        // Ensure the "socket" key exists in the specific socket response
        ASSERT_TRUE(socketData.contains("socket"));

        // Access the "socket" data
        auto specificSocket = socketData["socket"];

        // Validate that the ID matches
        ASSERT_EQ(specificSocket["id"].get<int>(), firstSocketId);
    }
    else
    {
        FAIL() << "The sockets array is empty.";
    }
}



// Test Canvas CRUD operations

TEST_F(APITest, CanvasCRUD)
{
    std::string canvasName = "Test Canvas " + std::to_string(std::time(nullptr));
    // Create canvas
    json canvasData = {
        {"id", -1}, // Allow the server to assign the ID
        {"name", canvasName},
        {"width", 100},
        {"height", 100}};

    auto createResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases"},
        cpr::Body{canvasData.dump()},
        jsonHeader, noPersistParam);
    ASSERT_EQ(createResponse.status_code, 201);

    // Parse the response to get the new ID
    auto createJson = json::parse(createResponse.text);
    ASSERT_TRUE(createJson.contains("id")); // Ensure the response contains "id"
    int newId = createJson["id"].get<int>();
    ASSERT_GT(newId, 0); // Verify the ID is valid

    // Read all canvases
    auto listResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases"}, noPersistParam);
    ASSERT_EQ(listResponse.status_code, 200);
    auto canvases = json::parse(listResponse.text);
    ASSERT_FALSE(canvases.empty());

    // Read specific canvas using the new ID
    auto getResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases/" + std::to_string(newId)},
                                noPersistParam);
    ASSERT_EQ(getResponse.status_code, 200);
    auto canvas = json::parse(getResponse.text);
    ASSERT_EQ(canvas["name"], canvasName);

    // Delete the canvas using the new ID
    auto deleteResponse = cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(newId)},
                                      noPersistParam);
    ASSERT_EQ(deleteResponse.status_code, 204);

    // Verify deletion
    auto verifyResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases/" + std::to_string(newId)},
                                   noPersistParam);
    ASSERT_EQ(verifyResponse.status_code, 404);
}

// Test Feature operations within a canvas

TEST_F(APITest, CanvasFeatureOperations)
{
    // First create a canvas
    json canvasData = {
        {"id", -1}, // Let the server assign the ID
        {"name", "Test Canvas " + std::to_string(std::time(nullptr))},
        {"width", 100},
        {"height", 100}};

    // Send the POST request and capture the response
    auto createCanvasResponse = cpr::Post(cpr::Url{BASE_URL + "/canvases"},
                                          cpr::Body{canvasData.dump()},
                                          jsonHeader, noPersistParam);

    // Assert the response status
    ASSERT_EQ(createCanvasResponse.status_code, 201); // Ensure the canvas was created successfully

    // Parse the response body to extract the new ID
    auto responseJson = json::parse(createCanvasResponse.text);
    int newCanvasId = responseJson["id"].get<int>();

    // Use the new ID in subsequent operations
    ASSERT_GT(newCanvasId, 0); // Validate the ID is a positive number

    // Create feature with all required fields
    json featureData = {
        {"hostName", "example-host"},
        {"friendlyName", "Test Feature"},
        {"port", 1234},
        {"width", 32},
        {"height", 16},
        {"offsetX", 50},
        {"offsetY", 50},
        {"reversed", false},
        {"channel", 1},
        {"redGreenSwap", false},
        {"clientBufferCount", 8}};

    auto createFeatureResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(newCanvasId) + "/features"},
        cpr::Body{featureData.dump()},
        jsonHeader, noPersistParam
    );

    ASSERT_EQ(createFeatureResponse.status_code, 201);

    auto featureInfo = json::parse(createFeatureResponse.text);
    int featureId = featureInfo["id"].get<int>();

    auto getCanvasResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases/" + std::to_string(newCanvasId)}, noPersistParam);

    ASSERT_EQ(getCanvasResponse.status_code, 200);
    auto canvasJson = json::parse(getCanvasResponse.text);
    ASSERT_TRUE(canvasJson.contains("features"));
    ASSERT_FALSE(canvasJson["features"].empty());

    auto features = canvasJson["features"];
    bool featureFound = false;

    for (const auto &feature : features)
    {
        if (feature["id"].get<int>() == featureId)
        {
            featureFound = true;
            ASSERT_TRUE(feature.contains("isConnected"));
            ASSERT_EQ(feature["friendlyName"], "Test Feature");
            ASSERT_EQ(feature["width"], 32);
            ASSERT_EQ(feature["height"], 16);
        }
    }

    ASSERT_TRUE(featureFound); // Ensure the feature was added to the canvas

    // Delete feature
    auto deleteFeatureResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(newCanvasId) + "/features/" + std::to_string(featureId)},
        noPersistParam);
    ASSERT_EQ(deleteFeatureResponse.status_code, 204);

    // Delete the canvas
    auto deleteCanvasResponse = cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(newCanvasId)},
                                            noPersistParam);
}

/* Causes a lot of logging of errors in the server
// Test error cases
TEST_F(APITest, ErrorHandling)
{
    // Test invalid canvas ID
    auto invalidCanvasResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases/999"}, noPersistParam);
    ASSERT_EQ(invalidCanvasResponse.status_code, 404);

    // Test invalid JSON in canvas creation
    auto invalidJsonResponse = cpr::Post(cpr::Url{BASE_URL + "/canvases"},
                                         cpr::Body{"invalid json"},
                                         jsonHeader, noPersistParam);
    ASSERT_EQ(invalidJsonResponse.status_code, 400);
}
*/

// Test multiple canvas operations
TEST_F(APITest, MultipleCanvasOperations)
{
    const int NUM_CANVASES = 10;
    std::vector<int> canvasIds;

    // Create multiple canvases simultaneously
    std::vector<std::future<cpr::Response>> createFutures;
    for (int i = 0; i < NUM_CANVASES; i++)
    {
        std::this_thread::sleep_for(milliseconds(stddelay)); // Small delay

        createFutures.push_back(std::async(std::launch::async, [i]()
        {
            json canvasData = {
                {"id", -1},
                {"name", "Test Canvas " + std::to_string(i) + std::to_string(std::time(nullptr))},
                {"width", 100},
                {"height", 100}
            };

            return cpr::Post(cpr::Url{BASE_URL + "/canvases"},
                             cpr::Body{canvasData.dump()},
                             jsonHeader, noPersistParam);
        }));
    }

    // Collect canvas IDs and verify creation
    for (auto &future : createFutures)
    {
        auto response = future.get();
        ASSERT_EQ(response.status_code, 201);
        auto json_response = json::parse(response.text);
        canvasIds.push_back(json_response["id"].get<int>());
    }

    // Verify all canvases exist
    auto listResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases"},
                                 noPersistParam);
    ASSERT_EQ(listResponse.status_code, 200);
    auto canvases = json::parse(listResponse.text);
    ASSERT_GE(canvases.size(), (size_t) NUM_CANVASES);

    // Cleanup all canvases concurrently
    std::vector<std::future<cpr::Response>> deleteFutures;
    for (int id : canvasIds)
    {
        deleteFutures.push_back(std::async(std::launch::async, [id]()
        {
            return cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(id)},
                                noPersistParam);
        }));
    }

    // Verify all deletions
    for (auto &future : deleteFutures)
    {
        auto response = future.get();
        ASSERT_EQ(response.status_code, 204);
    }
}


// Test multiple feature operations within a single canvas
TEST_F(APITest, MultipleFeatureOperations)
{
    // Create a test canvas
    json canvasData = {
        {"id", -1},
        {"name", "Feature Stress Test Canvas " + std::to_string(std::time(nullptr))},
        {"width", 1000},
        {"height", 1000}};

    auto createCanvasResponse = cpr::Post(cpr::Url{BASE_URL + "/canvases"},
                                          cpr::Body{canvasData.dump()},
                                          jsonHeader, noPersistParam);

    ASSERT_EQ(createCanvasResponse.status_code, 201);
    auto canvasJson = json::parse(createCanvasResponse.text);
    int canvasId = canvasJson["id"].get<int>();

    const int NUM_FEATURES = 10;
    std::vector<int> featureIds;

    // Add multiple features concurrently
    std::vector<std::future<cpr::Response>> featureFutures;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        std::this_thread::sleep_for(milliseconds(stddelay)); // Small delay

        featureFutures.push_back(std::async(std::launch::async, [i, canvasId]()
        {
            json featureData = {
                {"hostName", "stress-host-" + std::to_string(i)},
                {"friendlyName", "Stress Feature " + std::to_string(i)},
                {"port", 1234 + i},
                {"width", 32},
                {"height", 16},
                {"offsetX", (i * 50) % 1000},
                {"offsetY", (i * 30) % 1000},
                {"reversed", false},
                {"channel", i % 4 + 1},
                {"redGreenSwap", false},
                {"clientBufferCount", 8}
            };

            return cpr::Post(cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/features"},
                             cpr::Body{featureData.dump()},
                             jsonHeader, noPersistParam);
        }));
    }

    // Collect and verify feature creation
    for (auto &future : featureFutures)
    {
        auto response = future.get();
        ASSERT_EQ(response.status_code, 201);
        auto featureJson = json::parse(response.text);
        featureIds.push_back(featureJson["id"].get<int>());
    }

    // Delete features concurrently
    std::vector<std::future<cpr::Response>> deleteFutures;
    for (int featureId : featureIds)
    {
        deleteFutures.push_back(std::async(std::launch::async, [canvasId, featureId]()
                                           { return cpr::Delete(
                                                 cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) +
                                                          "/features/" + std::to_string(featureId)},
                                                 noPersistParam); }));
    }

    // Verify feature deletions
    for (auto &future : deleteFutures)
    {
        auto response = future.get();
        ASSERT_EQ(response.status_code, 204);
    }

    // Cleanup canvas
    auto deleteCanvasResponse = cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
                                            noPersistParam);
    ASSERT_EQ(deleteCanvasResponse.status_code, 204);
}

// Test rapid creation/deletion cycles
TEST_F(APITest, RapidCreationDeletion)
{
    const int NUM_CYCLES = 10;
    const int NUM_CANVASES_PER_CYCLE = 10;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++)
    {
        std::vector<int> cycleCanvasIds;

        // Create canvases
        for (int i = 0; i < NUM_CANVASES_PER_CYCLE; i++)
        {
            std::this_thread::sleep_for(milliseconds(stddelay)); // Small delay

            json canvasData = {
                {"id", -1},
                { "name", "Cycle " + std::to_string(cycle) + " Canvas " + std::to_string(i) + " " + std::to_string(std::time(nullptr)) },
                {"width", 100},
                {"height", 100}};

            auto response = cpr::Post(cpr::Url{BASE_URL + "/canvases"},
                                      cpr::Body{canvasData.dump()},
                                      jsonHeader, noPersistParam);

            ASSERT_EQ(response.status_code, 201);
            auto jsonResponse = json::parse(response.text);
            cycleCanvasIds.push_back(jsonResponse["id"].get<int>());

            // Add a feature to each canvas
            json featureData = {
                {"hostName", "cycle-host"},
                {"friendlyName", "Cycle Feature"},
                {"port", 1234},
                {"width", 32},
                {"height", 16},
                {"offsetX", 50},
                {"offsetY", 50},
                {"reversed", false},
                {"channel", 1},
                {"redGreenSwap", false},
                {"clientBufferCount", 8}};

            auto featureResponse = cpr::Post(
                cpr::Url{BASE_URL + "/canvases/" + std::to_string(cycleCanvasIds.back()) + "/features"},
                cpr::Body{featureData.dump()},
                jsonHeader, noPersistParam);
            ASSERT_EQ(featureResponse.status_code, 201);
        }

        // Delete all canvases in this cycle
        for (int id : cycleCanvasIds)
        {
            std::this_thread::sleep_for(milliseconds(stddelay)); // Small delay

            auto response = cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(id)},
                                        noPersistParam);
            ASSERT_EQ(response.status_code, 204);
        }
    }
}

TEST_F(APITest, CanvasFeatureEffectWithSchedule)
{
    // Create a canvas
    json canvasData = {
        {"id", -1},
        {"name", "Test Canvas " + std::to_string(std::time(nullptr))},
        {"width", 100},
        {"height", 100}
    };

    auto createCanvasResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases"},
        cpr::Body{canvasData.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createCanvasResponse.status_code, 201);
    auto canvasJson = json::parse(createCanvasResponse.text);
    int canvasId = canvasJson["id"].get<int>();

    // Create a feature
    json featureData = {
        {"hostName", "test-host"},
        {"friendlyName", "Test Feature"},
        {"port", 1234},
        {"width", 32},
        {"height", 16},
        {"offsetX", 10},
        {"offsetY", 10},
        {"reversed", false},
        {"channel", 1},
        {"redGreenSwap", false},
        {"clientBufferCount", 8}
    };

    auto createFeatureResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/features"},
        cpr::Body{featureData.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createFeatureResponse.status_code, 201);
    auto featureJson = json::parse(createFeatureResponse.text);
    int featureId = featureJson["id"].get<int>();

    // Create first effect with schedule
    json effect1Data = {
        {"type", "SolidColorFill"},
        {"name", "Test Effect 1"},
        {"color", {
            {"red", 255},
            {"green", 0},
            {"blue", 0}
        }},
        {"schedule", {
            {"daysOfWeek", 0x3E},  // Monday through Friday
            {"startTime", "09:00:00"},
            {"stopTime", "17:00:00"}
        }}
    };

    auto createEffect1Response = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/effects"},
        cpr::Body{effect1Data.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createEffect1Response.status_code, 201);

    // Create second effect
    json effect2Data = {
        {"type", "SolidColorFill"},
        {"name", "Test Effect 2"},
        {"color", {
            {"red", 0},
            {"green", 0},
            {"blue", 255}
        }}
    };

    auto createEffect2Response = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/effects"},
        cpr::Body{effect2Data.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createEffect2Response.status_code, 201);

    // Get the canvas to verify effects
    auto getCanvasResponse = cpr::Get(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(getCanvasResponse.status_code, 200);
    auto canvasWithEffects = json::parse(getCanvasResponse.text);

    // Verify effects through effectsManager
    ASSERT_TRUE(canvasWithEffects.contains("effectsManager"));
    const auto& effectsManager = canvasWithEffects["effectsManager"];
    ASSERT_TRUE(effectsManager.contains("fps"));
    ASSERT_EQ(effectsManager["fps"].get<int>(), 30); // Default FPS
    ASSERT_TRUE(effectsManager.contains("effects"));
    const auto& effects = effectsManager["effects"];
    ASSERT_EQ(effects.size(), (size_t) 2);

    // Verify basic structure and schedule
    for (const auto& effect : effects) {
        ASSERT_TRUE(effect.contains("type"));
        ASSERT_TRUE(effect.contains("name"));
        ASSERT_TRUE(effect.contains("color"));
        ASSERT_TRUE(effect["color"].contains("r"));
        ASSERT_TRUE(effect["color"].contains("g"));
        ASSERT_TRUE(effect["color"].contains("b"));

        // If this is the first effect (with schedule)
        if (effect["name"] == "Test Effect 1") {
            ASSERT_TRUE(effect.contains("schedule"));
            ASSERT_TRUE(effect["schedule"].contains("daysOfWeek"));
            ASSERT_EQ(effect["schedule"]["daysOfWeek"], 0x3E);
            ASSERT_TRUE(effect["schedule"].contains("startTime"));
            ASSERT_EQ(effect["schedule"]["startTime"], "09:00:00");
            ASSERT_TRUE(effect["schedule"].contains("stopTime"));
            ASSERT_EQ(effect["schedule"]["stopTime"], "17:00:00");
        }
        else {
            ASSERT_FALSE(effect.contains("schedule"));
        }
    }

    // Clean up
    auto deleteFeatureResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/features/" + std::to_string(featureId)},
        noPersistParam
    );
    ASSERT_EQ(deleteFeatureResponse.status_code, 204);

    auto deleteCanvasResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(deleteCanvasResponse.status_code, 204);
}

TEST_F(APITest, CanvasUpdatePreservesFeatures)
{
    json canvasData = {
        {"id", -1},
        {"name", "Editable Canvas " + std::to_string(std::time(nullptr))},
        {"width", 64},
        {"height", 8}
    };

    auto createCanvasResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases"},
        cpr::Body{canvasData.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createCanvasResponse.status_code, 201);
    const int canvasId = json::parse(createCanvasResponse.text)["id"].get<int>();

    json featureData = {
        {"hostName", "update-test-host"},
        {"friendlyName", "Editable Feature"},
        {"port", 1234},
        {"width", 32},
        {"height", 4},
        {"offsetX", 10},
        {"offsetY", 2},
        {"reversed", false},
        {"channel", 1},
        {"redGreenSwap", false},
        {"clientBufferCount", 16}
    };

    auto createFeatureResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/features"},
        cpr::Body{featureData.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createFeatureResponse.status_code, 201);
    const int featureId = json::parse(createFeatureResponse.text)["id"].get<int>();

    auto getCanvasResponse = cpr::Get(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(getCanvasResponse.status_code, 200);

    auto canvasJson = json::parse(getCanvasResponse.text);
    canvasJson["name"] = "Updated Canvas";
    canvasJson["width"] = 96;
    canvasJson["height"] = 12;
    canvasJson["effectsManager"]["fps"] = 48;
    canvasJson["effectsManager"]["running"] = false;

    auto updateResponse = cpr::Put(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        cpr::Body{canvasJson.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(updateResponse.status_code, 200);

    auto updatedCanvas = json::parse(updateResponse.text);
    ASSERT_EQ(updatedCanvas["name"], "Updated Canvas");
    ASSERT_EQ(updatedCanvas["width"], 96);
    ASSERT_EQ(updatedCanvas["height"], 12);
    ASSERT_EQ(updatedCanvas["effectsManager"]["fps"], 48);
    ASSERT_EQ(updatedCanvas["features"].size(), static_cast<size_t>(1));
    ASSERT_EQ(updatedCanvas["features"][0]["id"], featureId);
    ASSERT_EQ(updatedCanvas["features"][0]["friendlyName"], "Editable Feature");

    auto deleteCanvasResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(deleteCanvasResponse.status_code, 204);
}

TEST_F(APITest, EffectCatalogExposesDefinitions)
{
    auto response = cpr::Get(
        cpr::Url{BASE_URL + "/effects/catalog"},
        noPersistParam
    );

    ASSERT_EQ(response.status_code, 200);
    auto catalogJson = json::parse(response.text);
    ASSERT_TRUE(catalogJson.contains("effects"));
    ASSERT_FALSE(catalogJson["effects"].empty());

    bool foundSolidColorFill = false;
    bool foundPaletteEditorField = false;

    for (const auto &effect : catalogJson["effects"])
    {
        if (effect["label"] == "Solid Color Fill")
            foundSolidColorFill = true;

        if (effect["label"] == "Palette Scroll")
        {
            for (const auto &field : effect["fields"])
            {
                if (field["path"] == "palette.colors")
                    foundPaletteEditorField = true;
            }
        }
    }

    ASSERT_TRUE(foundSolidColorFill);
    ASSERT_TRUE(foundPaletteEditorField);
}

TEST_F(APITest, CanvasPixelsEndpointReturnsBinaryFrame)
{
    json canvasData = {
        {"id", -1},
        {"name", "Pixels Test Canvas " + std::to_string(std::time(nullptr))},
        {"width", 12},
        {"height", 5}
    };

    auto createCanvasResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases"},
        cpr::Body{canvasData.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createCanvasResponse.status_code, 201);
    const int canvasId = json::parse(createCanvasResponse.text)["id"].get<int>();

    auto pixelsResponse = cpr::Get(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/pixels"},
        noPersistParam
    );
    ASSERT_EQ(pixelsResponse.status_code, 200);

    const auto &body = pixelsResponse.text;
    ASSERT_GE(body.size(), static_cast<size_t>(6));

    const auto readU16LE = [&body](size_t offset) -> uint16_t
    {
        const auto lo = static_cast<uint8_t>(body[offset]);
        const auto hi = static_cast<uint8_t>(body[offset + 1]);
        return static_cast<uint16_t>((hi << 8) | lo);
    };

    const uint16_t width = readU16LE(0);
    const uint16_t height = readU16LE(2);
    const uint16_t fps = readU16LE(4);

    ASSERT_EQ(width, 12);
    ASSERT_EQ(height, 5);
    ASSERT_GT(fps, 0);

    const size_t expectedSize = 6 + static_cast<size_t>(width) * height * 3;
    ASSERT_EQ(body.size(), expectedSize);

    auto deleteCanvasResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(deleteCanvasResponse.status_code, 204);
}

TEST_F(APITest, ReversedMatrixFeatureMirrorsRowsForHardware)
{
    FeatureMappingCanvas canvas(3, 2);
    auto feature = make_shared<LEDFeature>(
        "127.0.0.1",
        "Matrix Mapping Feature",
        49152,
        3,
        2,
        0,
        0,
        true
    );
    canvas.AddFeature(feature);

    canvas.Graphics().SetPixel(0, 0, CRGB(10, 0, 0));
    canvas.Graphics().SetPixel(1, 0, CRGB(0, 20, 0));
    canvas.Graphics().SetPixel(2, 0, CRGB(0, 0, 30));
    canvas.Graphics().SetPixel(0, 1, CRGB(40, 0, 0));
    canvas.Graphics().SetPixel(1, 1, CRGB(0, 50, 0));
    canvas.Graphics().SetPixel(2, 1, CRGB(0, 0, 60));

    const auto pixels = feature->GetPixelData();
    const vector<uint8_t> expected = {
        0, 0, 30, 0, 20, 0, 10, 0, 0,
        0, 0, 60, 0, 50, 0, 40, 0, 0
    };

    ASSERT_EQ(pixels, expected);
}

TEST_F(APITest, CanvasRuntimeControlEndpointsSupportWebUiFlow)
{
    json canvasData = {
        {"id", -1},
        {"name", "Runtime Test Canvas " + std::to_string(std::time(nullptr))},
        {"width", 16},
        {"height", 8}
    };

    auto createCanvasResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases"},
        cpr::Body{canvasData.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(createCanvasResponse.status_code, 201);
    const int canvasId = json::parse(createCanvasResponse.text)["id"].get<int>();

    json effectOne = {
        {"type", "SolidColorFill"},
        {"name", "Runtime Effect 1"},
        {"color", {{"red", 255}, {"green", 0}, {"blue", 0}}}
    };

    json effectTwo = {
        {"type", "SolidColorFill"},
        {"name", "Runtime Effect 2"},
        {"color", {{"red", 0}, {"green", 255}, {"blue", 0}}}
    };

    auto addEffectOneResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/effects"},
        cpr::Body{effectOne.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(addEffectOneResponse.status_code, 201);

    auto addEffectTwoResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/effects"},
        cpr::Body{effectTwo.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(addEffectTwoResponse.status_code, 201);

    auto stopResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/stop"},
        cpr::Body{json{{"canvasIds", json::array({canvasId})}}.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(stopResponse.status_code, 200);

    auto startResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/start"},
        cpr::Body{json{{"canvasIds", json::array({canvasId})}}.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(startResponse.status_code, 200);

    auto setCurrentEffectResponse = cpr::Post(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId) + "/currentEffect"},
        cpr::Body{json{{"effectIndex", 1}}.dump()},
        jsonHeader, noPersistParam
    );
    ASSERT_EQ(setCurrentEffectResponse.status_code, 200);

    auto getCanvasResponse = cpr::Get(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(getCanvasResponse.status_code, 200);

    const auto canvasJson = json::parse(getCanvasResponse.text);
    ASSERT_TRUE(canvasJson.contains("effectsManager"));
    ASSERT_TRUE(canvasJson["effectsManager"].contains("currentEffectIndex"));
    ASSERT_EQ(canvasJson["effectsManager"]["currentEffectIndex"].get<int>(), 1);

    auto deleteCanvasResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(deleteCanvasResponse.status_code, 204);
}
