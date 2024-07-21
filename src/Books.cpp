#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <string>
#include "httplib.h"
#include "json.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <atomic>
#include "thread_safe_queue.h"

using json = nlohmann::json;

struct Movie {
    std::string title;
    std::string producer;
    std::string release_year;
    std::string runtime;
    std::vector<std::string> genres;
    std::vector<std::string> cast;
    std::string poster_url;
    GLuint texture_id = 0;
};

struct ImageData {
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
    GLuint texture_id = 0;
};

std::mutex mtx;
std::condition_variable cv;
std::queue<std::string> image_queue;
std::atomic<bool> image_thread_running(true);
std::map<std::string, ImageData> textureMap;

GLFWwindow* window;

bool FetchMovieInfo(Movie &movie, std::string &image_url, bool &connection_error);
void LoadImageFromUrl(const std::string& url);
void ImageLoadingThread();
void FetchMovieList(const std::string &title, const std::string &year, ThreadSafeQueue<Movie> &movie_queue, bool &connection_error);
void CreateTexture(const std::string& url);

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a GLFW window
    window = glfwCreateWindow(1280, 720, "Movie Info", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Start the image loading thread
    std::thread image_thread(ImageLoadingThread);

    // Variables for ImGui input
    char title_input[256] = "";
    char year_input[5] = "";  // Add this for year input
    bool movie_not_found = false;
    bool connection_error = false;
    Movie selected_movie;
    std::string image_url;
    int selected_movie_index = -1;
    std::vector<Movie> movie_list;
    std::atomic<bool> search_in_progress(false);
    ThreadSafeQueue<Movie> movie_queue;
    std::thread fetcher_thread;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create ImGui window
        ImGui::Begin("Movie Information", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::SetWindowPos(ImVec2(0, 0));
        ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);

        // Set up two columns
        ImGui::Columns(2, "MovieColumns", true);

        // In the left column, update the movie details display:
        ImGui::BeginChild("MovieDetails", ImVec2(0, -1), true);
        if (selected_movie_index != -1 && !selected_movie.title.empty()) {
            ImGui::Text("Title: %s", selected_movie.title.c_str());
            ImGui::Text("Year: %s", selected_movie.release_year.c_str());
            ImGui::Text("Director: %s", selected_movie.producer.c_str());
            ImGui::Text("Runtime: %s", selected_movie.runtime.c_str());

            if (!selected_movie.genres.empty()) {
                ImGui::Text("Genres:");
                for (const auto& genre : selected_movie.genres) {
                    ImGui::BulletText("%s", genre.c_str());
                }
            }

            if (!selected_movie.cast.empty()) {
                ImGui::Text("Cast:");
                for (const auto& actor : selected_movie.cast) {
                    ImGui::BulletText("%s", actor.c_str());
                }
            }

            // Display movie poster
            if (!image_url.empty() && textureMap.find(image_url) != textureMap.end()) {
                const auto& imageData = textureMap[image_url];
                if (imageData.texture_id != 0) {
                    ImGui::Image((void*)(intptr_t)imageData.texture_id, ImVec2(200, 300));
                } else if (imageData.data != nullptr) {
                    // Create texture on the main thread
                    CreateTexture(image_url);
                    ImGui::Text("Image loaded, creating texture...");
                } else {
                    ImGui::Text("Image is still loading...");
                }
            } else {
                ImGui::Text("Image not available");
            }
        } else {
            ImGui::Text("Select a movie to see details");
        }
        ImGui::EndChild();


        ImGui::NextColumn();
// Right column: Search and movie list
        ImGui::BeginChild("SearchAndList", ImVec2(0, -1), true);

// Title search
        ImGui::Text("Search by Title:");
        bool triggerSearch = ImGui::InputText("Title", title_input, IM_ARRAYSIZE(title_input), ImGuiInputTextFlags_EnterReturnsTrue);

// Year search
        ImGui::Text("Year (optional):");
        ImGui::InputText("Year", year_input, IM_ARRAYSIZE(year_input)); // No flag needed, as it does not trigger search alone.

        ImGui::SameLine();
        if (ImGui::Button("Search") || triggerSearch) {
            if (fetcher_thread.joinable()) {
                fetcher_thread.join();
            }
            movie_list.clear();
            selected_movie = Movie();
            image_url.clear();
            movie_not_found = false;
            connection_error = false;
            selected_movie_index = -1;
            search_in_progress.store(true);

            // Trigger fetching movie list based on title and use year as a filter
            fetcher_thread = std::thread([&]() {
                FetchMovieList(title_input, year_input, movie_queue, connection_error);
            });
        }

        // Process movies from the queue
        if (search_in_progress.load()) {
            Movie movie;
            while (movie_queue.pop(movie)) {
                movie_list.push_back(movie);
            }
            if (movie_queue.empty() && movie_queue.is_finished()) {
                search_in_progress.store(false);
                if (movie_list.empty()) {
                    movie_not_found = true;
                } else if (movie_list.size() == 1) {
                    // Automatically select and display the movie if it's the only one in the list
                    selected_movie_index = 0;
                    selected_movie = movie_list[0];
                    image_url.clear();

                    // Fetch detailed movie info
                    bool fetch_success = FetchMovieInfo(selected_movie, image_url, connection_error);
                    if (fetch_success) {
                        // Update the movie in the list with the fetched details
                        movie_list[selected_movie_index] = selected_movie;

                        // Load the image if it's not already loaded
                        if (!image_url.empty()) {
                            std::unique_lock<std::mutex> lock(mtx);
                            if (textureMap.find(image_url) == textureMap.end()) {
                                image_queue.push(image_url);
                                cv.notify_one();
                            }
                        }
                    } else {
                        std::cerr << "Failed to fetch movie details. Please try again." << std::endl;
                    }
                }
            }
        }

// Display search results or messages
        if (search_in_progress.load()) {
            ImGui::Text("Searching...");
        } else if (!movie_list.empty()) {
            if (movie_list.size() > 1) {
                ImGui::Text("Select a movie:");
                // Create a child window for the scrollable list
                ImGui::BeginChild("MovieList", ImVec2(0, 200), true);
                for (int i = 0; i < movie_list.size(); ++i) {
                    if (ImGui::Selectable(movie_list[i].title.c_str(), selected_movie_index == i)) {
                        selected_movie_index = i;
                        selected_movie = movie_list[i];
                        image_url.clear();

                        // Fetch detailed movie info when selected
                        bool fetch_success = FetchMovieInfo(selected_movie, image_url, connection_error);
                        if (fetch_success) {
                            // Update the movie in the list with the fetched details
                            movie_list[selected_movie_index] = selected_movie;

                            // Load the image if it's not already loaded
                            if (!image_url.empty()) {
                                std::unique_lock<std::mutex> lock(mtx);
                                if (textureMap.find(image_url) == textureMap.end()) {
                                    image_queue.push(image_url);
                                    cv.notify_one();
                                }
                            }
                        } else {
                            ImGui::Text("Failed to fetch movie details. Please try again.");
                        }
                    }
                }
                ImGui::EndChild();
            } else {
                // If there's only one movie, it's already selected and displayed
                ImGui::Text("Displaying the only movie found:");
                ImGui::Text("%s (%s)", selected_movie.title.c_str(), selected_movie.release_year.c_str());
            }
        } else if (movie_not_found) {
            ImGui::Text("No movies found. Please try another search.");
        } else if (connection_error) {
            ImGui::Text("Connection error occurred. Please check your internet connection and try again.");
        }

        ImGui::EndChild();

        ImGui::Columns(1);
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

bool FetchMovieInfo(Movie &movie, std::string &image_url, bool &connection_error) {
    std::string api_key = "67880361"; // Replace with your OMDb API key
    std::string encoded_title = httplib::detail::encode_url(movie.title);
    std::string url = "/?t=" + encoded_title + "&y=" + movie.release_year + "&apikey=" + api_key;

    httplib::Client cli("https://www.omdbapi.com");
    auto res = cli.Get(url);

    if (!res) {
        connection_error = true;
        return false;
    }

    if (res->status == 200) {
        json response = json::parse(res->body);
        if (response["Response"] == "True") {
            movie.title = response.value("Title", movie.title);
            movie.producer = response.value("Director", "Unknown");
            movie.release_year = response.value("Year", movie.release_year);
            movie.runtime = response.value("Runtime", "Unknown");

            // Handle Genre
            movie.genres.clear();
            std::string genre_str = response.value("Genre", "");
            std::istringstream ss(genre_str);
            std::string genre;
            while (std::getline(ss, genre, ',')) {
                movie.genres.push_back(genre);
            }

            // Handle Cast
            movie.cast.clear();
            std::string cast_str = response.value("Actors", "");
            std::istringstream cast_ss(cast_str);
            std::string actor;
            while (std::getline(cast_ss, actor, ',')) {
                movie.cast.push_back(actor);
            }

            // Handle Poster
            if (response.contains("Poster") && response["Poster"] != "N/A") {
                image_url = response["Poster"].get<std::string>();
                movie.poster_url = image_url;
            } else {
                image_url = "";
                movie.poster_url = "";
            }

            connection_error = false;
            return true;
        }
    }
    connection_error = false;
    return false;
}

// Modify the LoadImageFromUrl function:
void LoadImageFromUrl(const std::string& url) {
    httplib::SSLClient cli("m.media-amazon.com");
    cli.set_follow_location(true);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(10);

    httplib::Headers headers = {
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"}
    };

    std::string path = url.substr(url.find("/images"));

    auto res = cli.Get(path, headers);
    if (res && res->status == 200) {
        int width, height, channels;
        unsigned char* data = stbi_load_from_memory(
                reinterpret_cast<const unsigned char*>(res->body.c_str()),
                (int)res->body.size(), &width, &height, &channels, 0
        );

        if (data) {
            std::unique_lock<std::mutex> lock(mtx);
            textureMap[url] = {data, width, height, channels, 0};
            glfwPostEmptyEvent();
        } else {
            std::cerr << "Failed to decode image data. STB Error: " << stbi_failure_reason() << std::endl;
        }
    } else {
        std::cerr << "Failed to download image. Status: " << (res ? res->status : 0) << std::endl;
    }
}

// Modify the CreateTexture function:
void CreateTexture(const std::string& url) {
    if (textureMap.find(url) != textureMap.end()) {
        ImageData& imageData = textureMap[url];
        if (imageData.texture_id == 0 && imageData.data != nullptr) {
            glGenTextures(1, &imageData.texture_id);
            glBindTexture(GL_TEXTURE_2D, imageData.texture_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageData.width, imageData.height, 0,
                         imageData.channels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, imageData.data);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(imageData.data);
            imageData.data = nullptr;
        }
    }
}


void ImageLoadingThread() {
    while (image_thread_running) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return !image_queue.empty() || !image_thread_running; });

        if (!image_thread_running) break;

        std::string url = image_queue.front();
        image_queue.pop();
        lock.unlock();

        LoadImageFromUrl(url);
    }
}

void FetchMovieList(const std::string &title, const std::string &year, ThreadSafeQueue<Movie> &movie_queue, bool &connection_error) {
    std::string api_key = "766745cb"; // Replace with your OMDb API key
    std::string encoded_title = httplib::detail::encode_url(title);
    std::string url = "/?s=" + encoded_title + "&type=movie&apikey=" + api_key;

    httplib::Client cli("https://www.omdbapi.com");
    auto res = cli.Get(url);

    if (!res) {
        connection_error = true;
        return;
    }

    if (res->status == 200) {
        json response = json::parse(res->body);
        if (response["Response"] == "True" && response.contains("Search")) {
            for (const auto& item : response["Search"]) {
                Movie movie;
                movie.title = item.value("Title", "Unknown");
                movie.release_year = item.value("Year", "Unknown");
                movie.poster_url = item.value("Poster", "");

                // Apply year filter here if specified
                if (year.empty() || movie.release_year == year) {
                    movie_queue.push(movie);
                }
            }
            connection_error = false;
        } else {
            connection_error = true;
        }
    } else {
        connection_error = true;
    }

    movie_queue.setFinished();
}
