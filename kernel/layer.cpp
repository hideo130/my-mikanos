#include "layer.hpp"

Layer::Layer(unsigned int id) : id_{id} {}

Layer &Layer::Move(Vector2D<int> pos)
{
    pos_ = pos;
    return *this;
}

Layer &Layer::MoveRelative(Vector2D<int> pos_diff)
{
    pos_ += pos_diff;
    return *this;
}

void Layer::DrawTo(FrameBuffer &writer) const
{
    if (window_)
    {
        window_->DrawTo(writer, pos_);
    }
}

unsigned int Layer::ID() const
{
    return id_;
}

Layer &Layer::SetWindow(const std::shared_ptr<Window> &window)
{
    window_ = window;
    return *this;
}

Layer &LayerManager::NewLayer()
{
    ++latest_id_;
    return *layers_.emplace_back(new Layer{latest_id_});
}

Layer *LayerManager::FindLayer(unsigned int id)
{
    auto pred = [id](const std::unique_ptr<Layer> &elem)
    {
        return elem->ID() == id;
    };
    auto it = std::find_if(layers_.begin(), layers_.end(), pred);
    if (it == layers_.end())
    {
        return nullptr;
    }
    return it->get();
}

void LayerManager::Move(unsigned int id, Vector2D<int> new_position)
{
    FindLayer(id)->Move(new_position);
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff)
{
    FindLayer(id)->MoveRelative(pos_diff);
}

void LayerManager::SetWriter(FrameBuffer *screen)
{
    screen_ = screen;
}

void LayerManager::Draw() const
{
    for (auto layer : layer_stack_)
    {
        layer->DrawTo(*screen_);
    }
}

void LayerManager::Draw(const Rectangle<int> &area) const
{
    for (auto layer : layer_stack_)
    {
        layer->DrawTo(*screen_, area);
    }
}

void LayerManager::Draw(unsigned int id)const{
    bool draw = false;
    Rectangle<int>window_area;
    // skip drawing background of window that have spicified id
    for(auto layer: layer_stack_){
        if(layer->ID() == id){
            window_area.size = layer->GetWindow()->Size();
            window_area.pos = layer->GetPosition();
            draw = true;
        }
        if(draw){
            layer->DrawTo(*screen_, window_area);
        }
    }
}


void LayerManager::Hide(unsigned int id)
{
    auto layer = FindLayer(id);
    auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    if (pos != layer_stack_.end())
    {
        layer_stack_.erase(pos);
    }
}

void LayerManager::UpDown(unsigned int id, int new_height)
{
    if (new_height < 0)
    {
        Hide(id);
        return;
    }

    if (new_height > layer_stack_.size())
    {
        new_height = layer_stack_.size();
    }

    auto layer = FindLayer(id);
    auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);

    auto new_pos = layer_stack_.begin() + new_height;

    // if layer is not displyed, then layer is not contained layer_stack_
    // so old_pos indicate end, and we add layer stack to display.
    if (old_pos == layer_stack_.end())
    {
        layer_stack_.insert(new_pos, layer);
        return;
    }

    // porcess when layer is already displayed.
    if (new_pos == layer_stack_.end())
    {
        --new_pos;
    }
    layer_stack_.erase(old_pos);
    layer_stack_.insert(new_pos, layer);
}

LayerManager *layer_manager;