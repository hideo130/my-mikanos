#pragma once

#include <array>

#include "frame_buffer.hpp"
#include "graphics.hpp"
#include "message.hpp"
#include "window.hpp"

class Layer
{
public:
    Layer(unsigned int id = 0);
    unsigned int ID() const;

    Layer &SetDraggable(bool draggable);
    Layer &SetWindow(const std::shared_ptr<Window> &window);

    Vector2D<int> GetPosition() const;
    std::shared_ptr<Window> GetWindow() const;

    bool IsDraggable() const;
    Layer &Move(Vector2D<int> pos);
    Layer &MoveRelative(Vector2D<int> pos_diff);

    void DrawTo(FrameBuffer &screen, const Rectangle<int> &area) const;

private:
    unsigned int id_;
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
    bool draggable_{false};
};

/**
 * @brief Manage multiple layer
 *
 */
class LayerManager
{
public:
    void SetWriter(FrameBuffer *screen);

    Layer &NewLayer();

    void Draw(unsigned int id) const;
    void Draw(const Rectangle<int> &area) const;

    Layer *FindLayerByPosition(Vector2D<int> mouse_position, unsigned int mouse_layer_id) const;

    /**
     * @brief Move layer position to specify absolute position
     */
    void Move(unsigned int id, Vector2D<int> new_pos);
    /**
     * @brief Move layer position to specify relative position
     * Does not redraw.
     */
    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);
    void UpDown(unsigned int id, int new_height);
    void Hide(unsigned int id);
    Layer *FindLayer(unsigned int id);
    int GetHeight(unsigned int id);

private:
    FrameBuffer *screen_{nullptr};
    mutable FrameBuffer back_buffer_{};
    std::vector<std::unique_ptr<Layer>> layers_{};
    std::vector<Layer *> layer_stack_{};
    unsigned int latest_id_{0};
};

extern LayerManager *layer_manager;

class ActiveLayer
{
public:
    ActiveLayer(LayerManager &manager);
    void SetMouseLayer(unsigned int mouse_layer);
    void Activate(unsigned int layer_id);
    unsigned int GetActive() const { return active_layer_; }

private:
    LayerManager &manager_;
    unsigned int active_layer_{0};
    unsigned int mouse_layer_{0};
};

extern ActiveLayer *active_layer;

void InitializeLayer();
void ProcessLayerMessage(const Message &msg);
