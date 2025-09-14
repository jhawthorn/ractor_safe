require 'test_helper'

class TestQueue < Minitest::Test
  def test_basic_operations
    queue = RactorSafe::Queue.new
    
    assert queue.empty?
    assert_equal 0, queue.size
    
    queue.push(:item1)
    queue << :item2
    queue.enq(:item3)
    
    refute queue.empty?
    assert_equal 3, queue.size
    
    assert_equal :item1, queue.try_pop
    assert_equal :item2, queue.try_pop
    assert_equal :item3, queue.try_pop
    
    assert queue.empty?
    assert_nil queue.try_pop
  end
  
  def test_shareable_between_ractors
    queue = RactorSafe::Queue.new
    
    # Verify it's shareable but not frozen
    assert Ractor.shareable?(queue)
    refute queue.frozen?
    
    # Producer ractor
    producer = Ractor.new(queue) do |q|
      10.times do |i|
        q.push(i)
      end
      :done
    end
    
    # Wait for producer to finish first
    assert_equal :done, producer.value
    
    # Now consume all items
    results = []
    10.times do
      results << queue.pop
    end
    
    assert_equal 10, results.length
    assert_equal (0..9).to_a.sort, results.sort
  end
  
  def test_concurrent_producers_consumers
    queue = RactorSafe::Queue.new
    
    # Multiple producers
    producers = 5.times.map do |i|
      Ractor.new(queue, i) do |q, producer_id|
        20.times do |j|
          q.push(:"item_#{producer_id}_#{j}")
        end
      end
    end
    
    # Multiple consumers
    consumers = 3.times.map do
      Ractor.new(queue) do |q|
        results = []
        33.times do # 100 items / 3 consumers = ~33 each
          item = q.try_pop
          results << item if item
        end
        results
      end
    end
    
    producers.each(&:value)
    
    all_results = consumers.map(&:value).flatten
    
    # Should have processed most/all items
    assert all_results.length >= 90
  end
  
  def test_rejects_non_shareable_values
    queue = RactorSafe::Queue.new
    
    assert_raises(ArgumentError) do
      queue.push("mutable_string")
    end
    
    # Shareable values should work
    queue.push(:symbol)
    queue.push(42)
    queue.push("frozen".freeze)
    
    assert_equal 3, queue.size
  end
  
  def test_clear
    queue = RactorSafe::Queue.new
    
    queue.push(:a)
    queue.push(:b)
    queue.push(:c)
    
    assert_equal 3, queue.size
    queue.clear
    assert_equal 0, queue.size
    assert queue.empty?
  end
  
  def test_aliases
    queue = RactorSafe::Queue.new
    
    # Test push aliases
    queue << :a
    queue.enq(:b)
    queue.push(:c)
    
    # Test pop aliases  
    assert_equal :a, queue.deq
    assert_equal :b, queue.shift
    assert_equal :c, queue.pop
    
    assert_equal queue.size, queue.length
  end
end
