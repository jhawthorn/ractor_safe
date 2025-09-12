require 'test_helper'

class TestHashMap < Minitest::Test
  def test_basic_operations
    map = RactorSafe::HashMap.new
    
    assert_nil map[:foo]
    
    map[:foo] = :bar
    assert_equal :bar, map[:foo]
    
    assert_equal 1, map.size
    assert map.has_key?(:foo)
    assert map.key?(:foo)
    assert map.include?(:foo)
    
    assert_equal :bar, map.delete(:foo)
    assert_nil map[:foo]
    assert_equal 0, map.size
  end
  
  def test_shareable_between_ractors
    map = RactorSafe::HashMap.new
    
    # Verify it's shareable
    assert Ractor.shareable?(map)
    
    # Test concurrent access from multiple Ractors
    map[:counter] = 0
    
    ractors = 10.times.map do |i|
      Ractor.new(map, i) do |shared_map, index|
        100.times do |j|
          key = :"key_#{index}_#{j}"
          shared_map[key] = j
        end
        shared_map
      end
    end
    
    ractors.each(&:value)
    
    # Should have 1000 entries (10 ractors * 100 keys each) + 1 counter
    assert_equal 1001, map.size
  end
  
  def test_concurrent_writes
    map = RactorSafe::HashMap.new
    
    ractors = 100.times.map do
      Ractor.new(map) do |shared_map|
        1000.times do |i|
          shared_map[:shared_key] = i
        end
      end
    end
    
    ractors.each(&:value)
    
    # The value should be between 0 and 999
    assert_includes 0..999, map[:shared_key]
  end
  
  def test_clear
    map = RactorSafe::HashMap.new
    map[:a] = 1
    map[:b] = 2
    
    assert_equal 2, map.size
    map.clear
    assert_equal 0, map.size
    assert_nil map[:a]
    assert_nil map[:b]
  end
  
  def test_rejects_non_shareable_keys
    map = RactorSafe::HashMap.new
    
    assert_raises(ArgumentError) do
      map["mutable_string"] = 42
    end
  end
  
  def test_rejects_non_shareable_values
    map = RactorSafe::HashMap.new
    
    assert_raises(ArgumentError) do
      map[:key] = "mutable_string"
    end
  end
  
  def test_accepts_shareable_values
    map = RactorSafe::HashMap.new
    
    # Frozen strings are shareable
    map[:key] = "frozen".freeze
    assert_equal "frozen", map[:key]
    
    # Symbols are shareable
    map[:symbol_key] = :symbol_value
    assert_equal :symbol_value, map[:symbol_key]
    
    # Numbers are shareable
    map[:number] = 42
    assert_equal 42, map[:number]
    
    # Frozen arrays with shareable contents are shareable
    map[:array] = [1, 2, 3].freeze
    assert_equal [1, 2, 3], map[:array]
  end
  
  def test_ruby_comparison_semantics
    map = RactorSafe::HashMap.new
    
    # Test that equal strings with same content map to same key
    key1 = "test".freeze
    key2 = "test".freeze
    
    map[key1] = 1
    map[key2] = 2
    
    # Should overwrite since keys are equal
    assert_equal 2, map[key1]
    assert_equal 2, map[key2]
    assert_equal 1, map.size
  end
  
  def test_array_key_comparison
    map = RactorSafe::HashMap.new
    
    # Test that arrays with same content are treated as same key
    key1 = [1, 2, 3].freeze
    key2 = [1, 2, 3].freeze
    
    map[key1] = :first
    map[key2] = :second
    
    # Should overwrite since arrays have same content
    assert_equal :second, map[key1]
    assert_equal :second, map[key2]
    assert_equal 1, map.size
  end
end