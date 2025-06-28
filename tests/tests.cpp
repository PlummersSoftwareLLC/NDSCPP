#include <gtest/gtest.h>
#include <cpr/cpr.h> // Modern C++ HTTP library
#include <../json.hpp>

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

const std::string BASE_URL = "http://localhost:7777/api";

const int stddelay = 5;

const auto jsonHeader = cpr::Header{{"Content-Type", "application/json"}};
const auto noPersistParam = cpr::Parameters{{"nopersist", "1"}};


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
    ASSERT_EQ(deleteResponse.status_code, 200);

    // Verify deletion
    auto verifyResponse = cpr::Get(cpr::Url{BASE_URL + "/canvases/" + std::to_string(newId)},
                                   noPersistParam);
    ASSERT_EQ(verifyResponse.status_code, 400);
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

    ASSERT_EQ(createFeatureResponse.status_code, 200);

    auto featureInfo = json::parse(createFeatureResponse.text);
    int featureId = featureInfo["id"].get<int>();

    // Delete feature
    auto deleteFeatureResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(newCanvasId) + "/features/" + std::to_string(featureId)},
        noPersistParam);
    ASSERT_EQ(deleteFeatureResponse.status_code, 200);

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
        ASSERT_EQ(response.status_code, 200);
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
        ASSERT_EQ(response.status_code, 200);
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
        ASSERT_EQ(response.status_code, 200);
    }

    // Cleanup canvas
    auto deleteCanvasResponse = cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
                                            noPersistParam);
    ASSERT_EQ(deleteCanvasResponse.status_code, 200);
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
            ASSERT_EQ(featureResponse.status_code, 200);
        }

        // Delete all canvases in this cycle
        for (int id : cycleCanvasIds)
        {
            std::this_thread::sleep_for(milliseconds(stddelay)); // Small delay

            auto response = cpr::Delete(cpr::Url{BASE_URL + "/canvases/" + std::to_string(id)},
                                        noPersistParam);
            ASSERT_EQ(response.status_code, 200);
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
    ASSERT_EQ(createFeatureResponse.status_code, 200);
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
    ASSERT_EQ(createEffect1Response.status_code, 200);

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
    ASSERT_EQ(createEffect2Response.status_code, 200);

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
    ASSERT_EQ(deleteFeatureResponse.status_code, 200);

    auto deleteCanvasResponse = cpr::Delete(
        cpr::Url{BASE_URL + "/canvases/" + std::to_string(canvasId)},
        noPersistParam
    );
    ASSERT_EQ(deleteCanvasResponse.status_code, 200);
}
